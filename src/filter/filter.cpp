#include "src/filter/filter.hh"
#include "src/constants.hh"
#include "src/error.hh"
#include "src/filter/error.hh"
#include "src/util.hh"
#include "src/filter/util.hh"

#include <algorithm>
#include <cassert>
#include <cerrno>
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
	#include <libavcodec/avcodec.h>
	#include <libavcodec/codec.h>
	#include <libavcodec/codec_par.h>
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

	class VideoFileSource : public PacketSource {
		private:
			AVFormatContext *m_ctx;
			Span                     m_span;
			int64_t                  m_dts_start;
			TsInfo                   m_dts_end_info; //< The dts and duration of the last video packet (dts-wise) in each stream
			TsInfo                   m_pts_end_info; //< The pts and duration of the last video packet (pts-wise) in each stream
			int64_t                  m_video_idx;

			bool                     m_requires_transcoding;
			AVCodecContext          *m_decoder;
			AVCodecContext          *m_encoder;
			AVFrame                 *m_frame_buf;

			const AVCodecParameters *m_video_params;

			AVIOContext     *m_io_ctx;
		public:
			VideoFileSource() = delete;

			VideoFileSource(const std::string& path, Span span, const AVCodecParameters *output_params)
				: m_ctx(avformat_alloc_context())
				, m_span(span)
				, m_decoder(nullptr)
				, m_encoder(nullptr)
				, m_frame_buf(nullptr)
				, m_video_params(output_params)
				, m_io_ctx(nullptr)
			{

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

				AVCodecParameters *input_params = m_ctx->streams[m_video_idx]->codecpar;
				if(!m_video_params) {
					m_video_params = input_params;
				}

				m_requires_transcoding = m_video_params && !util::codecs_are_compatible(m_video_params, input_params);
				if(m_requires_transcoding) {
					const AVRational native_timebase = m_ctx->streams[m_video_idx]->time_base;

					m_decoder = util::create_decoder(m_span, input_params, native_timebase);
					m_encoder = util::create_encoder(m_span, m_video_params, native_timebase);
				}
			}

			bool next_pkt(AVPacket **p_packet) {
				AVPacket *const packet = *p_packet;

				if(m_requires_transcoding) {
					for(;;) {
						int res = util::transcode_receive_packet(m_decoder, m_encoder, &m_frame_buf, packet);
						if(res == AVERROR_EOF) {
							return false;
						} else if(res != AVERROR(EAGAIN)) {
							error::handle_ffmpeg_error(m_span, res);
							break;
						}

						if(!util::read_packet_from_stream(m_span, m_ctx, m_video_idx, packet)) {
							error::handle_ffmpeg_error(m_span,
								avcodec_send_packet(m_decoder, nullptr)
							);
							continue;
						}

						error::handle_ffmpeg_error(m_span,
							avcodec_send_packet(m_decoder, packet)
						);
						av_packet_unref(packet);
					}
				} else {
					if(!util::read_packet_from_stream(m_span, m_ctx, m_video_idx, packet)) {
						return false;
					}
				}

				packet->stream_index = 0;

				const AVRational old_timebase = m_ctx->streams[packet->stream_index]->time_base;

				av_packet_rescale_ts(packet, old_timebase, constants::TIMEBASE);

				return true;
			}

			const AVCodecParameters *video_codec() {
				return m_video_params;
			}

			std::span<AVStream *> av_streams() {
				return std::span<AVStream *>(m_ctx->streams, m_ctx->nb_streams);
			}

			TsInfo dts_end_info() const {
				return m_dts_end_info;
			}

			int64_t dts_start() const {
				return m_dts_start;
			}

			TsInfo pts_end_info() const {
				return m_pts_end_info;
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
				if(m_decoder) {
					avcodec_free_context(&m_decoder);
				}
				if(m_encoder) {
					avcodec_free_context(&m_encoder);
				}
				if(m_frame_buf) {
					av_frame_free(&m_frame_buf);
				}
			}

			VideoFileSource(VideoFileSource&& old)
				: m_ctx(old.m_ctx)
				, m_span(old.m_span)
				, m_requires_transcoding(old.m_requires_transcoding)
				, m_decoder(old.m_decoder)
				, m_encoder(old.m_encoder)
				, m_frame_buf(old.m_frame_buf)
				, m_video_params(old.m_video_params)
				, m_io_ctx(old.m_io_ctx)
			{
				old.m_ctx = nullptr;
				old.m_io_ctx = nullptr;
				old.m_decoder = nullptr;
				old.m_encoder = nullptr;
				old.m_frame_buf = nullptr;
				old.m_requires_transcoding = false;
			}

		private:
			// Walks through the file to calculate `m_dts_start`, `m_dts_end_info`, `m_pts_end_info`, and `video_idx`
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

				m_dts_end_info = {0, 0};
				m_pts_end_info = {0, 0};
				m_dts_start = 0;

				AVPacket *pkt = av_packet_alloc();

				while(int res = av_read_frame(ctx, pkt) != AVERROR_EOF) {
					error::handle_ffmpeg_error(m_span, res);

					if(pkt->stream_index == m_video_idx) {
						av_packet_rescale_ts(pkt, ctx->streams[pkt->stream_index]->time_base, constants::TIMEBASE);

						if(pkt->dts >= m_dts_end_info.ts) {
							TsInfo& packet_info = m_dts_end_info;

							packet_info.ts = pkt->dts;
							packet_info.duration = pkt->duration;
						}

						if(pkt->dts < m_dts_start) {
							m_dts_start = pkt->dts;
						}

						if(pkt->pts >= m_pts_end_info.ts) {
							TsInfo& packet_info = m_pts_end_info;

							packet_info.ts = pkt->pts;
							packet_info.duration = pkt->duration;
						}
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

	std::unique_ptr<PacketSource> VideoFile::get_pkts(Span s, const AVCodecParameters *params) const {
		return std::make_unique<VideoFileSource>(m_path, s, params);
	}

	class ConcatSource : public PacketSource {
		public:
			ConcatSource() = delete;

			ConcatSource(std::span<const Spanned<const VFilter&>> videos, Span span, const AVCodecParameters *params)
				: m_idx(0)
				, m_span(span) {
				for(const auto& video : videos) {
					m_videos.push_back(video->get_pkts(video.span, params));
					if(!params) {
						params = m_videos.back()->video_codec();
					}
				}

				calculate_ts_offsets();
			}

			bool next_pkt(AVPacket **p_packet) {
				for(;;) {
					if(m_idx >= m_videos.size()) {
						return false;
					}

					if(m_videos[m_idx]->next_pkt(p_packet)) {
						break;
					} else {
						m_idx++;
					}
				}

				AVPacket *packet = *p_packet;

				packet->pts += m_pts_offsets[m_idx];
				packet->dts += m_dts_offsets[m_idx];

				return true;
			}

			const AVCodecParameters *video_codec() {
				return m_videos[0]->video_codec();
			}

			TsInfo dts_end_info() const {
				TsInfo ts_info = m_videos.back()->dts_end_info();

				ts_info.ts += m_dts_offsets.back();
				return ts_info;
			}

			int64_t dts_start() const {
				return m_videos.front()->dts_start() + m_videos.front()->dts_start();
			}

			TsInfo pts_end_info() const {
				TsInfo ts_info = m_videos.back()->pts_end_info();

				ts_info.ts += m_pts_offsets.back();
				return ts_info;
			}

		private:
			void calculate_ts_offsets() {
				assert(!m_videos.empty());

				m_dts_offsets.reserve(m_videos.size());
				m_pts_offsets.reserve(m_videos.size());

				{ // Calculate pts offsets //
					size_t pts_offset = 0;

					for(auto& vid : m_videos) {
						m_pts_offsets.push_back(pts_offset);

						TsInfo pts_info = vid->pts_end_info();
						pts_offset += pts_info.ts + pts_info.duration;
					}
				}

				{ // Calculate dts offsets //
					m_dts_offsets.push_back(m_pts_offsets.back());;

					if(m_videos.size() == 1) {
						return;
					}

					int64_t end = m_pts_offsets.back() + m_videos.back()->dts_start();

					for(size_t i = m_videos.size() - 1; i-- > 0;) {
						const auto& video = m_videos[i];

						const TsInfo  old_end   = video->dts_end_info();
						const int64_t old_start = video->dts_start();

						const int64_t new_pts_start = m_pts_offsets[i];
						const int64_t new_end   = end - old_end.duration;
						const int64_t new_start = new_end - (old_end.ts - old_start);

						const int64_t max_new_start = new_pts_start + old_start;

						if(new_start > max_new_start) {
							m_dts_offsets.push_back(max_new_start - old_start);
							end = max_new_start;
							continue;
						}

						m_dts_offsets.push_back(new_start - old_start);
						end = new_start;
					}

					std::reverse(m_dts_offsets.begin(), m_dts_offsets.end());
				}
			}

			std::vector<std::unique_ptr<PacketSource>> m_videos;
			std::vector<int64_t>                       m_dts_offsets; //< How much the dts of every packet in the videos should be offset
			std::vector<int64_t>                       m_pts_offsets; //< How much the pts of every packet in the videos should be offset
			size_t                                     m_idx;

			[[maybe_unused]]
			Span                                       m_span;
	};

	std::unique_ptr<PacketSource> Concat::get_pkts(Span s, const AVCodecParameters *params) const {
		return std::make_unique<ConcatSource>(m_videos, s, params);
	}
}
