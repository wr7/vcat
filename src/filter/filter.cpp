#include "src/filter/filter.hh"
#include "src/constants.hh"
#include "src/error.hh"
#include "src/filter/error.hh"
#include "src/util.hh"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
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
	#include <libavcodec/packet.h>
	#include <libavformat/avformat.h>
	#include <libavformat/avio.h>
	#include <libavutil/mem.h>
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

	static int read_packet(void *opaque, uint8_t *buf, int buf_size) {
		FILE *avio_ctx = (FILE *) opaque;

		errno = 0;
		size_t a = fread(buf, 1, buf_size, avio_ctx);
		if (errno != 0) {
			return AVERROR(errno);
		}

		return a;
	}

	static int64_t seek_func(void *opaque, int64_t offset, int whence)
	{
		FILE *file = (FILE *) opaque;

		if (fseek(file, offset, whence) != 0) {
			return AVERROR(errno);
		}

		return ftell(file);
	}

	struct DtsInfo {
		int64_t dts;
		int64_t duration;
	};

	class VideoFileSource : public PacketSource {
		private:
			AVFormatContext                 *m_ctx;
			Span                             m_span;
			DtsInfo                          m_dts_end_info; //< The dts and duration of the last video packet (dts-wise) in each stream
			int64_t                          m_video_idx;

			AVIOContext                     *m_io_ctx;
		public:
			VideoFileSource() = delete;

			VideoFileSource(const std::string& path, Span span)
				: m_ctx(avformat_alloc_context())
				, m_span(span)
				, m_io_ctx(nullptr) {

				recreate_io_ctx(path);

				calculate_info(path);

				recreate_io_ctx(path);

				m_ctx->flags |= AVFMT_FLAG_SORT_DTS | AVFMT_FLAG_GENPTS | AVFMT_AVOID_NEG_TS_MAKE_ZERO;
				m_ctx->pb = m_io_ctx;

				error::handle_ffmpeg_error(m_span,
					avformat_open_input(&m_ctx, path.c_str(), NULL, NULL)
				);

				error::handle_ffmpeg_error(m_span,
					avformat_find_stream_info(m_ctx, NULL)
				);
			}

			bool next_pkt(AVPacket **p_packet) {
				start:
				int ret_code = av_read_frame(m_ctx, *p_packet);

				if(ret_code == AVERROR_EOF) {
					return false;
				}

				error::handle_ffmpeg_error(m_span, ret_code);

				AVPacket *const packet = *p_packet;

				if(packet->stream_index == m_video_idx) {
					packet->stream_index = 0;
				} else {
					av_packet_unref(packet);
					goto start;
				}

				const AVRational old_timebase = m_ctx->streams[packet->stream_index]->time_base;

				av_packet_rescale_ts(packet, old_timebase, constants::TIMEBASE);

				return true;
			}

			AVCodecParameters *video_codec() {
				return m_ctx->streams[m_video_idx]->codecpar;
			}

			std::span<AVStream *> av_streams() {
				return std::span<AVStream *>(m_ctx->streams, m_ctx->nb_streams);
			}

			~VideoFileSource() {
				if(m_ctx != nullptr) {
					avformat_close_input(&m_ctx);
				}

				if(m_io_ctx) {
					av_free(m_io_ctx->buffer);
					m_io_ctx->buffer = nullptr;

					fclose((FILE *) m_io_ctx->opaque);

					avio_context_free(&m_io_ctx);
				}
			}

			VideoFileSource(VideoFileSource&& old)
				: m_ctx(old.m_ctx)
				, m_span(old.m_span)
				, m_io_ctx(old.m_io_ctx)
			{
				old.m_ctx = nullptr;
				old.m_io_ctx = nullptr;
			}

		private:
			// Walks through the file to calculate `m_dts_end_info` and `video_idx`
			//
			// NOTE: this should be called before the main AVFormatContext is created,
			// and this will consume the AVIO context
			void calculate_info(const std::string& path) {
				AVFormatContext *ctx = avformat_alloc_context();
				ctx->pb = m_io_ctx;
				ctx->flags |= AVFMT_FLAG_GENPTS | AVFMT_AVOID_NEG_TS_MAKE_ZERO;

				error::handle_ffmpeg_error(m_span,
					avformat_open_input(&ctx, path.c_str(), NULL, NULL)
				);

				error::handle_ffmpeg_error(m_span,
					avformat_find_stream_info(ctx, NULL)
				);

				bool has_video_idx = false;
				for(size_t i = 0; i < ctx->nb_streams; i++) {
					if(ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
						m_video_idx = i;
						has_video_idx = true;
						break;
					}
				}

				if(!has_video_idx) {
					throw error::no_video(m_span, path);
				}

				AVPacket *pkt = av_packet_alloc();

				while(int res = av_read_frame(ctx, pkt) != AVERROR_EOF) {
					error::handle_ffmpeg_error(m_span, res);

					av_packet_rescale_ts(pkt, ctx->streams[pkt->stream_index]->time_base, constants::TIMEBASE);

					if(pkt->stream_index == m_video_idx && pkt->dts >= m_dts_end_info.dts) {
						DtsInfo& packet_info = m_dts_end_info;

						packet_info.dts = pkt->dts;
						packet_info.duration = pkt->duration;
					}

					av_packet_unref(pkt);
				}

				av_packet_free(&pkt);
				avformat_close_input(&ctx);
			}

			// Creates or re-creates the io context
			void recreate_io_ctx(const std::string& path) {
				if(!m_io_ctx) {
					errno = 0;
					FILE *fp = fopen(path.c_str(), "rb");  

					if(errno != 0) {
						throw error::failed_file_open(m_span, path);
					}

					uint8_t *buffer = (decltype(buffer)) av_malloc(4096);
					m_io_ctx = avio_alloc_context(buffer, 4096, 0, fp, read_packet, NULL, seek_func);
				} else {
					int res = fseek((FILE *) m_io_ctx->opaque, 0, SEEK_SET);
					if(res != 0) {
						throw error::failed_file_open(m_span, path);
					}

					uint8_t *buffer = m_io_ctx->buffer;
					int buffer_size = m_io_ctx->buffer_size;
					FILE *fp = (FILE *) m_io_ctx->opaque;

					avio_context_free(&m_io_ctx);
					m_io_ctx = avio_alloc_context(buffer, buffer_size, 0, fp, read_packet, NULL, seek_func);
				}
			}

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

			AVCodecParameters *video_codec() {
				return m_videos[0]->video_codec();
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
