#include "src/filter/filter.hh"
#include "src/error.hh"
#include "src/filter/error.hh"
#include "src/util.hh"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
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

		SHA256 hasher;

		while(f.good()) {
			size_t num_bytes = f.read(&buf[0], sizeof(buf)).gcount();

			hasher.add(&buf[0], num_bytes);
		}

		hasher.getHash(m_file_hash);

		if(f.bad()) {
			throw std::format("Failed to open file `{}`: {}", m_path, strerror(errno));
		}

		f.close();
	}

	std::string VideoFile::to_string() const {
		std::stringstream s;
		s << "VideoFile(\""
			<< PartiallyEscaped(m_path)
			<< "\")";

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

			std::span<AVStream *> streams() {
				return std::span<AVStream *>(m_ctx->streams, m_ctx->nb_streams);
			}

			~VideoFileSource() {
				avformat_close_input(&m_ctx);
			}

		private:
			AVFormatContext *m_ctx;
			Span             m_span;
	};

	std::unique_ptr<PacketSource> VideoFile::get_pkts(Span s) const {
		return std::make_unique<VideoFileSource>(m_path, s);
	}

	void Concat::hash(Hasher& hasher) const {
		hasher.add("_concat-filter_");
		const size_t start = hasher.pos();

		for(const auto& video : m_videos) {
			video.val->hash(hasher);
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
				<< indent(video.val->to_string())
				<< separator;
		}

		s << ")";

		return s.str();
	}

	std::string Concat::type_name() const {
		return "Concat";
	}

	class ConcatSource : public PacketSource {
		public:
			ConcatSource() = delete;

			ConcatSource(std::span<const Spanned<std::unique_ptr<VFilter>>> videos, Span span)
				: m_idx(0)
				, m_first_packet(true)
				, m_next(nullptr)
				, m_has_next(false)
				, m_prev_pkt_raw_pts(0)
				, m_span(span)
				, m_offset(0)
				, m_last_pkt_pts(0)
				, m_last_pkt_dur(0) {
				for(const auto& video : videos) {
					m_videos.push_back(Spanned(video->get()->get_pkts(video.span), video.span));
				}

				for(const auto& stream : m_videos[0]->get()->streams()) {
					AVStream *new_stream = (AVStream *) std::malloc(sizeof(*new_stream));
					std::memcpy(new_stream, stream, sizeof(*new_stream));

					new_stream->time_base = av_make_q(1, 90000);
					new_stream->start_time = 0;

					m_ostreams.push_back(new_stream);
				}
			}

			bool next_pkt(AVPacket **p_packet) {
				if(m_has_next) {
					std::swap(*p_packet, m_next);
					m_has_next = false;
					goto start;
				}

				for(;;) {
					if(m_idx >= m_videos.size()) {
						return false;
					}

					if(m_videos[m_idx]->get()->next_pkt(p_packet)) {
						break;
					} else {
						m_first_packet = true;
						m_prev_pkt_raw_pts = 0;
						m_offset = m_last_pkt_pts + m_last_pkt_dur;
						m_idx++;
					}
				}

				start:

				AVPacket *packet = *p_packet;

				const int64_t start_time = m_videos[m_idx]->get()->streams()[packet->stream_index]->start_time;

				if(start_time == AV_NOPTS_VALUE || packet->pts == AV_NOPTS_VALUE) {
					throw error::no_pts(m_span);
				}

				packet->pts -= start_time;
				packet->dts -= start_time;
				m_prev_pkt_raw_pts = packet->pts;

				assert((*p_packet)->pts >= 0);

				// Because some streams have negative dts, and naively allowing that may result in a non-
				// monotonous dts, we're going to reconstruct dts based on pts, but we don't want to break
				// B-frames.
				if(m_first_packet || (packet->flags & AV_PKT_FLAG_DISPOSABLE)) {
					packet->dts = packet->pts; // for non-reference frames/the first frame, this is really easy
				} else {
					if(!m_next) {
						m_next = av_packet_alloc();
						error::handle_ffmpeg_error(m_span, m_next ? 0 : AVERROR_UNKNOWN);
					}

					if(m_videos[m_idx]->get()->next_pkt(&m_next)) {
						m_has_next = true;

						if(m_next->flags & AV_PKT_FLAG_DISPOSABLE) {
							// we will assume that the next packet is a B frame and that it is the first frame (pts-wise)
							// that depends on this frame

							// ceiling average that does not overflow
							packet->dts = m_prev_pkt_raw_pts / 2 + m_next->pts / 2 + ((m_prev_pkt_raw_pts ^ m_next->pts) & 1);
						} else {
							packet->dts = packet->pts;
						}
					} else {
						packet->dts = packet->pts;
					}
				}

				const AVRational old_tb = m_videos[m_idx]->get()->streams()[packet->stream_index]->time_base;
				const AVRational new_tb = av_make_q(1, 90000); // C++ compound literals 😭

				packet->pts = av_rescale_q(packet->pts, old_tb, new_tb);
				packet->dts = av_rescale_q(packet->dts, old_tb, new_tb);
				packet->duration = av_rescale_q(packet->duration, old_tb, new_tb);

				packet->pts += m_offset;
				packet->dts += m_offset;

				if(packet->pts >= m_last_pkt_pts) {
					m_last_pkt_pts = packet->pts;
					m_last_pkt_dur = packet->duration;
				}

				m_first_packet = false;

				return true;
			}

			std::span<AVStream *> streams() {
				return m_ostreams;
			}

			~ConcatSource() {
				for(const auto& ostream : m_ostreams) {
					std::free(ostream);
				}

				if(m_next) {
					av_packet_free(&m_next);
				}
			}

		private:
			std::vector<Spanned<std::unique_ptr<PacketSource>>> m_videos;
			std::vector<AVStream *>                             m_ostreams;
			size_t                                              m_idx;

			bool                                                m_first_packet;
			AVPacket                                           *m_next;
			bool                                                m_has_next; // whether the next packet is stored in `m_next`
			int64_t                                             m_prev_pkt_raw_pts;

			[[maybe_unused]]
			Span                                                m_span;
			int64_t                                             m_offset;
			int64_t                                             m_last_pkt_pts;
			int64_t                                             m_last_pkt_dur;
	};

	std::unique_ptr<PacketSource> Concat::get_pkts(Span s) const {
		return std::make_unique<ConcatSource>(m_videos, s);
	}
}
