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
				: m_ctx(avformat_alloc_context())
				, m_span(span) {

				m_ctx->flags |= AVFMT_FLAG_SORT_DTS | AVFMT_FLAG_GENPTS | AVFMT_AVOID_NEG_TS_MAKE_ZERO;

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

			bool next_pkt(AVPacket **p_packet) {
				int ret_code = av_read_frame(m_ctx, *p_packet);

				if(ret_code == AVERROR_EOF) {
					return false;
				}

				error::handle_ffmpeg_error(m_span, ret_code);

				AVPacket *const packet = *p_packet;
				const AVRational old_timebase = m_ctx->streams[packet->stream_index]->time_base;

				av_packet_rescale_ts(packet, old_timebase, constants::TIMEBASE);

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

	std::unique_ptr<PacketSource> VideoFile::get_pkts(Span s) const {
		return std::make_unique<VideoFileSource>(m_path, s);
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
