#include "src/filter/filter.hh"
#include "src/constants.hh"
#include "src/error.hh"
#include "src/filter/error.hh"
#include "src/filter/params.hh"
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
#include <filesystem>
#include <format>
#include <iomanip>
#include <ios>
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
	#include <libavutil/frame.h>
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

	class VideoFilePktSource : public PacketSource {
		private:
			AVFormatContext *m_ctx;
			Span                     m_span;
			int64_t                  m_dts_start;
			TsInfo                   m_dts_end_info; //< The dts and duration of the last video packet (dts-wise) in each stream
			TsInfo                   m_pts_end_info; //< The pts and duration of the last video packet (pts-wise) in each stream
			int64_t                  m_video_idx;
			size_t                   m_pkt_no; //< The index of the current packet (starting at 0)

			// Some formats (ie mkv) do not support negative DTS and instead use `AV_NOPTS_VALUE`. This value
			// is used to reconstruct the negative DTS
			uint64_t                 m_dts_jump;

			util::VCatAVFile         m_file;
		public:
			VideoFilePktSource() = delete;

			VideoFilePktSource(const std::string& path, Span span)
				: m_ctx(avformat_alloc_context())
				, m_span(span)
				, m_pkt_no(0)
				, m_dts_jump(0)
				, m_file()
			{
				errno = 0;
				FILE *fp = fopen(path.c_str(), "rb");  

				if(errno != 0) {
					throw error::failed_file_open(m_span, path);
				}

				new(&m_file) util::VCatAVFile(fp);

				calculate_info(path);

				m_ctx->flags |= AVFMT_FLAG_SORT_DTS | AVFMT_FLAG_GENPTS | AVFMT_AVOID_NEG_TS_MAKE_ZERO;
				m_ctx->pb = m_file.get();

				error::handle_ffmpeg_error(m_span,
					avformat_open_input(&m_ctx, path.c_str(), NULL, NULL)
				);

				error::handle_ffmpeg_error(m_span,
					avformat_find_stream_info(m_ctx, NULL)
				);
			}

			bool next_pkt(AVPacket **p_packet) {
				AVPacket *const packet = *p_packet;

				int res = av_read_frame(m_ctx, *p_packet);
				if(res == AVERROR_EOF) {
					return false;
				}

				error::handle_ffmpeg_error(m_span, res);

				packet->stream_index = 0;

				const AVRational old_timebase = m_ctx->streams[packet->stream_index]->time_base;

				av_packet_rescale_ts(packet, old_timebase, constants::TIMEBASE);

				if(packet->dts == AV_NOPTS_VALUE) {
					packet->dts = m_dts_start + m_pkt_no * m_dts_jump;
				}

				m_pkt_no += 1;
				return true;
			}

			const AVCodecParameters *video_codec() {
				return m_ctx->streams[m_video_idx]->codecpar;
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

			~VideoFilePktSource() {
				if(m_ctx != nullptr) {
					avformat_close_input(&m_ctx);
				}
			}

			VideoFilePktSource(VideoFilePktSource&& old)
				: m_ctx(old.m_ctx)
				, m_span(old.m_span)
				, m_pkt_no(old.m_pkt_no)
				, m_dts_jump(old.m_dts_jump)
			{
				old.m_ctx = nullptr;
			}

		private:
			// Walks through the file to calculate `m_dts_start`, `m_dts_end_info`, `m_pts_end_info`, and `video_idx`
			//
			// NOTE: this should be called before the main AVFormatContext is created.
			void calculate_info(const std::string& path) {
				AVFormatContext *ctx = avformat_alloc_context();
				ctx->pb = m_file.get();
				ctx->flags |= AVFMT_FLAG_SORT_DTS | AVFMT_FLAG_GENPTS | AVFMT_AVOID_NEG_TS_MAKE_ZERO;

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

				std::optional<int64_t> first_defined_dts = {};
				int64_t num_undef_dts = 0;

				m_dts_end_info = {0, 0};
				m_pts_end_info = {0, 0};
				m_dts_start = 0;

				AVPacket *pkt = av_packet_alloc();

				while(int res = av_read_frame(ctx, pkt) != AVERROR_EOF) {
					error::handle_ffmpeg_error(m_span, res);

					if(pkt->stream_index == m_video_idx) {
						av_packet_rescale_ts(pkt, ctx->streams[pkt->stream_index]->time_base, constants::TIMEBASE);

						// Some formats (ie mkv) do not support negative DTS and instead use `AV_NOPTS_VALUE`
						//
						// Some of vcat's filters do not like this, so we need to manually reconstruct the DTS. That's
						// what this information is for.
						if(!first_defined_dts) {
							if(pkt->dts == AV_NOPTS_VALUE) {
								num_undef_dts++;
								continue;
							} else {
								// It would be genuinely insane if a file with this property did not start with a DTS of
								// zero, but we will handle this case anyway
								first_defined_dts = pkt->dts;

								if(pkt->duration > 0) {
									m_dts_jump = pkt->duration;
								} else {
									m_dts_jump = constants::FALLBACK_FRAME_RATE; // If worst comes to worst, we will just fall back to 60Hz
								}
							}
						}

						if(pkt->dts == AV_NOPTS_VALUE) {
							throw error::no_dts(m_span);
						}

						if(pkt->pts == AV_NOPTS_VALUE) {
							throw error::no_pts(m_span);
						}

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

				if(num_undef_dts > 0) {
					if(!first_defined_dts) {
						throw error::no_dts(m_span);
					}

					m_dts_start = *first_defined_dts - num_undef_dts * m_dts_jump;
				}

				av_packet_free(&pkt);
				avformat_close_input(&ctx);

				m_file.reset();
			}
	};

	class Decode : public FrameSource {
		public:
			Decode(Span s, std::unique_ptr<PacketSource>&& packet_src, const VideoParameters& video_params)
				: m_span(s)
				, m_packets(std::move(packet_src))
				, m_decoder(util::create_decoder(m_span, m_packets->video_codec()))
				, m_rescaler(util::Rescaler(m_span, m_decoder, video_params))
				, m_pkt_buf(av_packet_alloc())
			{
				error::handle_ffmpeg_error(m_span, m_pkt_buf ? 0 : AVERROR(ENOMEM));
			}

			bool next_frame(AVFrame **frame) {
				for(;;) {
					int res = avcodec_receive_frame(m_decoder, *frame);
					if(res == AVERROR_EOF) {
						return false;
					} else if(res != AVERROR(EAGAIN)) {
						error::handle_ffmpeg_error(m_span, res);
						m_rescaler.rescale(*frame);
						return true;
					}

					if(!m_packets->next_pkt(&m_pkt_buf)) {
						error::handle_ffmpeg_error(m_span,
							avcodec_send_packet(m_decoder, nullptr)
						);
						continue;
					}

					error::handle_ffmpeg_error(m_span,
						avcodec_send_packet(m_decoder, m_pkt_buf)
					);

					av_packet_unref(m_pkt_buf);
				}
			}

			~Decode() {
				avcodec_free_context(&m_decoder);
				av_packet_free(&m_pkt_buf);
			}

		private:
			Span                          m_span;
			std::unique_ptr<PacketSource> m_packets;
			AVCodecContext               *m_decoder;
			util::Rescaler                m_rescaler;
			AVPacket                     *m_pkt_buf;
	};

	std::unique_ptr<PacketSource> encode(Span span, const VideoParameters& params, const VFilter& filter) {
		std::string hash;
		{
			Hasher hasher;

			hasher.add("_cached-video_");

			const size_t start = hasher.pos();

			params.hash(hasher);
			filter.hash(hasher);

			hasher.add(static_cast<uint64_t>(hasher.pos() - start));

			hash = hasher.into_string();
		}

		try {
			std::filesystem::create_directory("./vcat-cache");
		} catch(const std::filesystem::filesystem_error& e) {
			throw error::failed_cache_directory(span, e.what());
		}

		std::string cached_name = "./vcat-cache/" + hash + ".mkv";

		if(std::filesystem::exists(cached_name)) {
			return std::make_unique<VideoFilePktSource>(cached_name, span);
		}

		std::string tmp_cached_name = "./vcat-cache/~" + hash + ".mkv";

		AVFormatContext *output = nullptr;
		error::handle_ffmpeg_error(span,
			avformat_alloc_output_context2(&output, nullptr, nullptr, tmp_cached_name.c_str())
		);

		AVStream *ostream = avformat_new_stream(output, nullptr);

		error::handle_ffmpeg_error(span,
			ostream ? 0 : AVERROR_UNKNOWN
		);

		AVCodecContext *encoder = util::create_encoder(span, params);

		error::handle_ffmpeg_error(span,
			avcodec_parameters_from_context(ostream->codecpar, encoder)
		);

		ostream->time_base = constants::TIMEBASE;

		if(!(output->flags & AVFMT_NOFILE)) {
			error::handle_ffmpeg_error(span,
				avio_open(&output->pb, tmp_cached_name.c_str(), AVIO_FLAG_WRITE)
			);
		}

		error::handle_ffmpeg_error(span,
			avformat_write_header(output, nullptr)
		);

		AVFrame  *frame  = av_frame_alloc();
		AVPacket *packet = av_packet_alloc();

		error::handle_ffmpeg_error(span, frame  ? 0 : AVERROR_UNKNOWN);
		error::handle_ffmpeg_error(span, packet ? 0 : AVERROR_UNKNOWN);

		std::unique_ptr<FrameSource> frames = filter.get_frames(span, params);

		int res;
		while((res = avcodec_receive_packet(encoder, packet)) != AVERROR_EOF) {
			if(res == AVERROR(EAGAIN)) {
				if(frames->next_frame(&frame)) {
					error::handle_ffmpeg_error(span,
						avcodec_send_frame(encoder, frame)
					);

					av_frame_unref(frame);
				} else {
					avcodec_send_frame(encoder, nullptr);
				}

				continue;
			}

			error::handle_ffmpeg_error(span, res);

			packet->pos = -1;
			packet->stream_index = 0;

			av_packet_rescale_ts(packet, constants::TIMEBASE, ostream->time_base);

			av_interleaved_write_frame(output, packet);

			av_packet_unref(packet);
		}

		error::handle_ffmpeg_error(span,
			av_write_trailer(output)
		);

		av_packet_free(&packet);
		av_frame_free(&frame);

		if (!(output->oformat->flags & AVFMT_NOFILE)) {
			error::handle_ffmpeg_error(span,
				avio_closep(&output->pb)
			);
		}

		avformat_free_context(output);

		std::filesystem::rename(tmp_cached_name, cached_name);

		return std::make_unique<VideoFilePktSource>(cached_name, span);
	}

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

	std::unique_ptr<FrameSource> VideoFile::get_frames(Span span, const VideoParameters& params) const {
		return std::make_unique<Decode>(
			span,
			std::make_unique<VideoFilePktSource>(m_path, span),
			params
		);
	}

	std::unique_ptr<PacketSource> VFilter::get_pkts(Span span, const VideoParameters& params) const {
		return encode(span, params, *this);
	}

	class ConcatFrameSource : public FrameSource {
		public:
			ConcatFrameSource() = delete;
			ConcatFrameSource(std::span<const Spanned<const VFilter&>> videos, const VideoParameters& params)
				: m_idx(0)
				, m_last_pts(0)
				, m_last_dur(0)
				, m_pts_offset(0)
			{
				for(const auto& video : videos) {
					m_videos.push_back(video->get_frames(video.span, params));
				}
			}

			bool next_frame(AVFrame **p_frame) {
				if(m_idx >= m_videos.size()) {
					return false;
				}

				AVFrame *frame = *p_frame;

				if(!m_videos[m_idx]->next_frame(p_frame)) {
					m_idx += 1;
					m_pts_offset = m_last_pts + m_last_dur;
					return this->next_frame(p_frame);
				}

				frame->pts += m_pts_offset;

				if(frame->duration != 0) {
					m_last_dur = frame->duration;
				} else {
					assert(frame->pts >= m_last_pts);
					m_last_dur = frame->pts - m_last_pts;
				}

				m_last_pts = frame->pts;

				return true;
			}

		private:
			size_t  m_idx;
			int64_t m_last_pts;
			int64_t m_last_dur;
			int64_t m_pts_offset;
			std::vector<std::unique_ptr<FrameSource>> m_videos;
	};

	std::unique_ptr<FrameSource> Concat::get_frames(Span, const VideoParameters& params) const {
		return std::make_unique<ConcatFrameSource>(m_videos, params);
	}

	class ConcatPktSource : public PacketSource {
		public:
			ConcatPktSource() = delete;

			ConcatPktSource(std::span<const Spanned<const VFilter&>> videos, Span, const VideoParameters& params)
				: m_idx(0)
			{

				const AVCodecParameters *prev_param = nullptr;

				for(const auto& video : videos) {
					m_videos.push_back(video->get_pkts(video.span, params));
					const AVCodecParameters *cur_param = m_videos.back()->video_codec();

					if(prev_param) {
						assert(util::codecs_are_compatible(cur_param, prev_param)); // TODO: generate an actual error message
					}

					prev_param = cur_param;
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
	};

	std::unique_ptr<PacketSource> Concat::get_pkts(Span s, const VideoParameters& params) const {
		return std::make_unique<ConcatPktSource>(m_videos, s, params);
	}
}
