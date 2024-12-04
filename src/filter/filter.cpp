#include "src/filter/filter.hh"
#include "src/constants.hh"
#include "src/error.hh"
#include "src/filter/error.hh"
#include "src/util.hh"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

extern "C" {
	#include <libavformat/avformat.h>
	#include <libavcodec/packet.h>
	#include <libavutil/avutil.h>
	#include <libavutil/buffer.h>
	#include <libavutil/mathematics.h>
	#include <libavutil/rational.h>
	#include <libavutil/error.h>
}

namespace vcat::filter {
	void VideoFile::hash(Hasher& hasher) const {
		hasher.add("_videofile_");
		hasher.add(&m_file_hash, sizeof(m_file_hash));
		hasher.add((uint64_t) sizeof(m_file_hash));
	}

	// NOTE: throws `std::string` upon IO failure
	VideoFile::VideoFile(std::string&& path) : m_path(std::move(path)) {
		std::ifstream f;
		f.open(m_path, std::ios_base::in | std::ios_base::binary);

		char buf[512];

		f.rdbuf()->pubsetbuf(0, 0);

		Hasher hasher;

		while(f.good()) {
			size_t num_bytes = f.read(&buf[0], sizeof(buf)).gcount();

			hasher.add(&buf[0], num_bytes);
		}

		std::array<uint8_t, 32> hash = hasher.into_bin();
		std::copy_n(hash.begin(), hash.size(), m_file_hash);

		if(f.bad()) {
			throw std::format("Failed to open file `{}`: {}", m_path, strerror(errno));
		}

		f.close();
	}

	std::string VideoFile::to_string() const {
		std::stringstream s;
		s << "VideoFile(" << std::quoted(m_path) << ")";

		return s.str();
	}

	std::string VideoFile::type_name() const {
		return "VideoFile";
	}

	class VideoFileSource : public PacketSource {
		public:
			VideoFileSource() = delete;

			VideoFileSource(std::string_view path, Span span)
				: m_ctx(NULL)
				, m_span(span) {
				error::handle_ffmpeg_error(m_span,
					avformat_open_input(&m_ctx, path.data(), NULL, NULL)
				);

				error::handle_ffmpeg_error(m_span,
					avformat_find_stream_info(m_ctx, NULL)
				);

				m_codecs.reserve(av_streams().size());

				for(AVStream *stream : av_streams()) {
					m_codecs.push_back(stream->codecpar);
				}

				assert(m_ctx);
			}

			bool next_pkt(AVPacket **packet) {
				int ret_code = av_read_frame(m_ctx, *packet);

				if(ret_code == AVERROR_EOF) {
					return false;
				}

				error::handle_ffmpeg_error(m_span, ret_code);

				return true;
			}

			std::span<AVCodecParameters *> codecs() {
				return m_codecs;
			}

			std::span<AVStream *> av_streams() {
				return std::span<AVStream *>(m_ctx->streams, m_ctx->nb_streams);
			}

			~VideoFileSource() {
				if(m_ctx != nullptr) {
					avformat_close_input(&m_ctx);
				}
			}

			VideoFileSource(VideoFileSource&& old)
				: m_ctx(old.m_ctx)
				, m_codecs(std::move(old.m_codecs))
				, m_span(old.m_span)
			{
				old.m_ctx = nullptr;
			}

		private:
			AVFormatContext                 *m_ctx;
			std::vector<AVCodecParameters *> m_codecs;
			Span                             m_span;
	};

	void Concat::hash(Hasher& hasher) const {
		hasher.add("_concat-filter_");
		const size_t start = hasher.pos();

		for(const auto& video : m_videos) {
			video->hash(hasher);
		}

		hasher.add((uint64_t) (hasher.pos() - start));
	}

	std::string Concat::to_string() const {
		std::stringstream s;

		std::string_view separator = m_videos.size() > 1 ? ",\n" : "";

		s << "Concat(";

		if(m_videos.size() > 1) {
			s << '\n';
		}

		for(const auto& video : m_videos) {
			s
				<< indent(video->to_string())
				<< separator;
		}

		s << ")";

		return s.str();
	}

	std::string Concat::type_name() const {
		return "Concat";
	}

	// Recalculates the timestamps in a video so that:
	// 1. The pts and dts of the streams start at 0
	// 2. The timebase is 90kHz
	class NormalizeTimestamps : public PacketSource {
		public:
			NormalizeTimestamps(NormalizeTimestamps&) = delete;
			NormalizeTimestamps() = delete;

			NormalizeTimestamps(VideoFileSource&& video, Span s)
				: m_input(std::move(video))
				, m_first_packet(true)
				, m_next(nullptr)
				, m_has_next(false)
				, m_prev_pkt_raw_pts(0)
				, m_span(s) {}

			bool next_pkt(AVPacket **p_packet) {
				if(m_has_next) {
					std::swap(*p_packet, m_next);
					m_has_next = false;
				} else {
					if(!m_input.next_pkt(p_packet)) {
						return false;
					}
				}

				AVPacket *packet = *p_packet;

				const int64_t start_time = m_input.av_streams()[packet->stream_index]->start_time;

				if(start_time == AV_NOPTS_VALUE || packet->pts == AV_NOPTS_VALUE) {
					throw error::no_pts(m_span);
				}

				packet->pts -= start_time;
				packet->dts -= start_time;
				m_prev_pkt_raw_pts = packet->pts;

				assert((*p_packet)->pts >= 0);

				// reconstruct dts without breaking B-Frames
				if(m_first_packet || (packet->flags & AV_PKT_FLAG_DISPOSABLE)) {
					packet->dts = packet->pts; // for non-reference frames/the first frame, this is really easy
				} else {
					if(!m_next) {
						m_next = av_packet_alloc();
						error::handle_ffmpeg_error(m_span, m_next ? 0 : AVERROR_UNKNOWN);
					}

					if(m_input.next_pkt(&m_next)) {
						m_has_next = true;

						if(m_next->flags & AV_PKT_FLAG_DISPOSABLE) {
							// we will assume that the next packet is a B frame and that it is the first frame (pts-wise)
							// that depends on this frame

							// ceiling mean that does not overflow
							packet->dts = m_prev_pkt_raw_pts / 2 + m_next->pts / 2 + ((m_prev_pkt_raw_pts | m_next->pts) & 1);
						} else {
							packet->dts = packet->pts;
						}
					} else {
						packet->dts = packet->pts;
					}
				}

				const AVRational old_tb = m_input.av_streams()[packet->stream_index]->time_base;

				av_packet_rescale_ts(packet, old_tb, constants::TIMEBASE);

				m_first_packet = false;

				return true;
			}

			std::span<AVCodecParameters *> codecs() {
				return m_input.codecs();
			}

			~NormalizeTimestamps() {
				if(m_next) {
					av_packet_free(&m_next);
				}
			}

			NormalizeTimestamps(NormalizeTimestamps&& o)
				: m_input(std::move(o.m_input))
				, m_first_packet(o.m_first_packet)
				, m_next(o.m_next)
				, m_has_next(o.m_has_next)
				, m_prev_pkt_raw_pts(o.m_prev_pkt_raw_pts)
				, m_span(o.m_span)
			{

				o.m_next = nullptr;
			}

		private:
			VideoFileSource                  m_input;

			bool                             m_first_packet;
			AVPacket                        *m_next;
			bool                             m_has_next; // whether the next packet is stored in `m_next`
			int64_t                          m_prev_pkt_raw_pts;

			Span                             m_span;
	};

	std::unique_ptr<PacketSource> VideoFile::get_pkts(Span s) const {
		return std::make_unique<NormalizeTimestamps>(VideoFileSource(m_path, s), s);
	}

	class ConcatSource : public PacketSource {
		public:
			ConcatSource() = delete;

			ConcatSource(std::span<const Spanned<const VFilter&>> videos, Span span)
				: m_idx(0)
				, m_span(span)
				, m_offset(0)
				, m_last_pkt_pts(0)
				, m_last_pkt_dur(0) {
				for(const auto& video : videos) {
					m_videos.push_back(video->get_pkts(video.span));
				}
			}

			bool next_pkt(AVPacket **p_packet) {
				for(;;) {
					if(m_idx >= m_videos.size()) {
						return false;
					}

					if(m_videos[m_idx]->next_pkt(p_packet)) {
						break;
					} else {
						m_offset = m_last_pkt_pts + m_last_pkt_dur;
						m_idx++;
					}
				}

				AVPacket *packet = *p_packet;

				packet->pts += m_offset;
				packet->dts += m_offset;

				if(packet->pts >= m_last_pkt_pts) {
					m_last_pkt_pts = packet->pts;
					m_last_pkt_dur = packet->duration;
				}

				return true;
			}

			std::span<AVCodecParameters *> codecs() {
				return m_videos[0]->codecs();
			}

		private:
			std::vector<std::unique_ptr<PacketSource>> m_videos;
			size_t                                     m_idx;

			[[maybe_unused]]
			Span                                       m_span;
			int64_t                                    m_offset;
			int64_t                                    m_last_pkt_pts;
			int64_t                                    m_last_pkt_dur;
	};

	std::unique_ptr<PacketSource> Concat::get_pkts(Span s) const {
		return std::make_unique<ConcatSource>(m_videos, s);
	}
}
