#include "src/filter/filter.hh"
#include "src/constants.hh"
#include "src/error.hh"
#include "src/filter/error.hh"
#include "src/filter/params.hh"
#include "src/util.hh"
#include "src/filter/util.hh"

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavcodec/codec.h>
	#include <libavcodec/codec_par.h>
	#include <libavcodec/packet.h>
	#include <libavfilter/buffersink.h>
	#include <libavfilter/buffersrc.h>
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
	Rescaler::Rescaler(Span span, std::unique_ptr<FrameSource>&& src, const util::FrameInfo& info, const VideoParameters& output)
	: m_span(span)
	, m_src(std::move(src))
	, m_frame_no(-1)
	, m_frame_dur(0)
	, m_input(nullptr)
	, m_output(nullptr)
	, m_filter_graph(nullptr)
	{
		if(
			info.width   == output.width            &&
			info.height  == output.height           &&
			info.pix_fmt == constants::PIXEL_FORMAT &&
			!output.fixed_fps                       &&
			av_cmp_q(info.sar, constants::SAMPLE_ASPECT_RATIO) == 0
		) {
			return;
		}

		std::string filter_string = std::format(
				"format={},"
				"scale={}:{}:force_original_aspect_ratio=decrease,"
				"pad={}:{}:-1:-1",
				static_cast<int>(constants::PIXEL_FORMAT),
				output.width,
				output.height,
				output.width,
				output.height
		);

		if(output.fixed_fps) {
			filter_string += std::format(",fps={}", output.fps);

			const AVRational fps = av_d2q(output.fps, std::numeric_limits<int>::max());
			const AVRational dur_sec = av_inv_q(fps);

			m_frame_dur = av_rescale_q(1, dur_sec, constants::TIMEBASE);
			m_frame_dur = std::max(m_frame_dur, (decltype(m_frame_dur)) 1);
		}

		m_filter_graph = create_filtergraph(
			m_span,
			filter_string.c_str(),
			info,
			&m_input,
			&m_output
		);
	}

	bool Rescaler::next_frame(AVFrame **p_frame) {
		m_frame_no += 1;

		int res;
		while((res = av_buffersink_get_frame(m_output, *p_frame)) == AVERROR(EAGAIN)) {
			if(m_src->next_frame(p_frame)) {
				error::handle_ffmpeg_error(m_span,
					av_buffersrc_add_frame(m_input, *p_frame)
				);
			} else {
				error::handle_ffmpeg_error(m_span,
					av_buffersrc_add_frame(m_input, nullptr)
				);
			}
		}

		if(res == AVERROR_EOF) {
			return false;
		}

		error::handle_ffmpeg_error(m_span, res);

		if(m_frame_dur) {
			AVFrame *const frame = *p_frame;
			frame->pts = m_frame_no * m_frame_dur;
			frame->duration = m_frame_dur;
		}

		return true;
	}

	Rescaler::~Rescaler() {
		avfilter_graph_free(&m_filter_graph);
	}

	Decode::Decode(Span s, std::unique_ptr<PacketSource>&& packet_src)
		: m_span(s)
		, m_packets(std::move(packet_src))
		, m_decoder(util::create_decoder(m_span, m_packets->video_codec()))
		, m_pkt_buf(av_packet_alloc())
	{
		error::handle_ffmpeg_error(m_span, m_pkt_buf ? 0 : AVERROR(ENOMEM));
	}

	bool Decode::next_frame(AVFrame **frame) {
		for(;;) {
			int res = avcodec_receive_frame(m_decoder, *frame);
			if(res == AVERROR_EOF) {
				return false;
			} else if(res != AVERROR(EAGAIN)) {
				error::handle_ffmpeg_error(m_span, res);
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

	Decode::~Decode() {
		avcodec_free_context(&m_decoder);
		av_packet_free(&m_pkt_buf);
	}

	std::unique_ptr<PacketSource> encode(FilterContext& ctx, Span span, const VFilter& filter) {
		std::string hash;
		{
			Hasher hasher;

			hasher.add("_cached-video_");

			const size_t start = hasher.pos();

			ctx.vparams.hash(hasher);
			filter.hash(hasher);

			hasher.add(static_cast<uint64_t>(hasher.pos() - start));

			hash = hasher.into_string();
		}

		try {
			std::filesystem::create_directory("./vcat-cache");
		} catch(const std::filesystem::filesystem_error& e) {
			throw error::failed_cache_directory(span, e.what());
		}

		std::string cached_name = "./vcat-cache/" + hash + ".mp4";

		if(std::filesystem::exists(cached_name)) {
			return std::make_unique<VideoFilePktSource>(ctx, cached_name, span);
		}

		std::string tmp_cached_name = "./vcat-cache/~" + hash + ".mp4";

		AVFormatContext *output = nullptr;
		error::handle_ffmpeg_error(span,
			avformat_alloc_output_context2(&output, nullptr, nullptr, tmp_cached_name.c_str())
		);

		AVStream *ostream = avformat_new_stream(output, nullptr);

		error::handle_ffmpeg_error(span,
			ostream ? 0 : AVERROR_UNKNOWN
		);

		AVCodecContext *encoder = util::create_encoder(span, ctx.vparams);

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

		std::unique_ptr<FrameSource> frames = filter.get_frames(ctx, span);

		AVBufferPool *buffer_pool = av_buffer_pool_init(sizeof(int64_t), nullptr);

		int res;
		while((res = avcodec_receive_packet(encoder, packet)) != AVERROR_EOF) {
			if(res == AVERROR(EAGAIN)) {
				if(frames->next_frame(&frame)) {
					assert(frame->duration);

					frame->opaque_ref = av_buffer_pool_get(buffer_pool);
					*reinterpret_cast<int64_t *>(frame->opaque_ref->data) = frame->duration;

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
			// The x264 encoder will not fill in the packet duration
			// We must do that ourselves
			if(packet->opaque_ref && !packet->duration) {
				packet->duration = *reinterpret_cast<int64_t *>(packet->opaque_ref->data);
			}

			av_packet_rescale_ts(packet, constants::TIMEBASE, ostream->time_base);

			av_interleaved_write_frame(output, packet);

			av_packet_unref(packet);
		}

		av_buffer_pool_uninit(&buffer_pool);

		error::handle_ffmpeg_error(span,
			av_write_trailer(output)
		);

		av_packet_free(&packet);
		av_frame_free(&frame);

		avcodec_free_context(&encoder);

		if (!(output->oformat->flags & AVFMT_NOFILE)) {
			error::handle_ffmpeg_error(span,
				avio_closep(&output->pb)
			);
		}

		avformat_free_context(output);

		std::filesystem::rename(tmp_cached_name, cached_name);

		return std::make_unique<VideoFilePktSource>(ctx, cached_name, span);
	}

	std::unique_ptr<PacketSource> VFilter::get_pkts(FilterContext& ctx, Span span) const {
		return encode(ctx, span, *this);
	}
}
