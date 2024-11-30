#include "src/filter/filter.hh"
#include "src/error.hh"
#include "src/filter/error.hh"
#include "src/util.hh"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <deque>
#include <format>
#include <memory>
#include <sstream>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

extern "C" {
	#include <libavformat/avformat.h>
	#include <libavutil/error.h>
}

namespace dvel::filter {
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

			bool next_pkt(AVPacket *packet) {
				int ret_code = av_read_frame(m_ctx, packet);
				if(ret_code == AVERROR_EOF) {
					return false;
				}

				error::handle_ffmpeg_error(m_span, ret_code);

				return true;
			}

			std::vector<AVStream *> streams() {
				std::vector<AVStream *> retval;
				retval.resize(m_ctx->nb_streams);

				std::memcpy(retval.data(), m_ctx->streams, m_ctx->nb_streams * sizeof(AVStream *));

				return retval;
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
				: m_span(span)
				, m_offset(0)
				, m_last_pkt_pts(0)
				, m_last_pkt_dur(0) {
				for(const auto& video : videos) {
					m_videos.push_back(Spanned(video->get()->get_pkts(video.span), video.span));
					assert(m_videos.back()->get()->streams().size() == 1);
				}

				m_streams = m_videos[0]->get()->streams();
			}

			bool next_pkt(AVPacket *packet) {
				for(;;) {
					if(m_videos.empty()) {
						return false;
					}

					if(m_videos[0]->get()->next_pkt(packet)) {
						break;
					} else {
						m_offset = m_last_pkt_pts + m_last_pkt_dur;
						m_videos.pop_front();
					}
				}

				packet->pts += m_offset;
				packet->dts += m_offset;

				if(packet->pts >= m_last_pkt_pts) {
					m_last_pkt_pts = packet->pts;
					m_last_pkt_dur = packet->duration;
				}

				return true;
			}

			std::vector<AVStream *> streams() {
				return m_streams;
			}

		private:
			std::deque<Spanned<std::unique_ptr<PacketSource>>> m_videos;
			std::vector<AVStream *>                             m_streams;
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
