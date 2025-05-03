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
#include <format>
#include <iostream>
#include <limits>
#include <memory>
#include <numbers>
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
	#include <libavutil/channel_layout.h>
	#include <libavutil/mathematics.h>
	#include <libavutil/rational.h>
	#include <libavutil/error.h>
	#include <libavutil/frame.h>
}

namespace vcat::filter {
	FFMpegFilter::FFMpegFilter(Span span, std::vector<std::unique_ptr<FrameSource>>&& src, const char *filter, std::span<const util::SFrameInfo> input_info, StreamType output_type)
		: m_span(span)
		, m_src(std::move(src))
		, m_filter_graph(nullptr)
		, m_output(nullptr)
	{
		util::FilterGraphInfo graph_info = util::create_filtergraph(span, filter, input_info, output_type);

		m_filter_graph = graph_info.graph;
		m_inputs = std::move(graph_info.inputs);
		m_output = std::move(graph_info.output);
	}

	bool FFMpegFilter::next_frame(AVFrame **p_frame) {
		int res;
		while((res = av_buffersink_get_frame(m_output, *p_frame)) == AVERROR(EAGAIN)) {
			for(size_t i = 0; i < m_src.size(); i++) {
				if(m_src[i]->next_frame(p_frame)) {
					error::handle_ffmpeg_error(m_span,
						av_buffersrc_add_frame(m_inputs[i], *p_frame)
					);
				} else {
					error::handle_ffmpeg_error(m_span,
						av_buffersrc_add_frame(m_inputs[i], nullptr)
					);
				}
			}
		}

		if(res == AVERROR_EOF) {
			return false;
		}

		error::handle_ffmpeg_error(m_span, res);

		AVRational output_timebase = av_buffersink_get_time_base(m_output);

		AVFrame *const frame = *p_frame;

		frame->pts      = av_rescale_q(frame->pts,      output_timebase, constants::TIMEBASE);
		frame->duration = av_rescale_q(frame->duration, output_timebase, constants::TIMEBASE);

		return true;
	}

	FFMpegFilter::~FFMpegFilter() {
		avfilter_graph_free(&m_filter_graph);
	}

	std::unique_ptr<FrameSource> rescale(Span span, std::unique_ptr<FrameSource>&& src, const util::VFrameInfo& info, const VideoParameters& output) {
		if(
			info.width           == output.width                       &&
			info.height          == output.height                      &&
			info.pix_fmt         == constants::PIXEL_FORMAT            &&
			info.color_space     == constants::COLOR_SPACE             &&
			info.color_range     == constants::COLOR_RANGE             &&
			info.color_primaries == constants::COLOR_PRIMARIES         &&
			info.color_trc       == constants::COLOR_TRANSFER_FUNCTION &&
			!output.fixed_fps                                          &&
			av_cmp_q(info.sar, constants::SAMPLE_ASPECT_RATIO) == 0
		) {
			return std::move(src);
		}
		std::string filter_string = std::format(
				"colorspace="
					"space={}:"
					"trc={}:"
					"primaries={}:"
					"range={}:"
					"format={}"
				",",
				static_cast<int>(constants::COLOR_SPACE),
				static_cast<int>(constants::COLOR_TRANSFER_FUNCTION),
				static_cast<int>(constants::COLOR_PRIMARIES),
				static_cast<int>(constants::COLOR_RANGE),
				static_cast<int>(constants::PIXEL_FORMAT)
		);

		if(info.rotation_degrees != 0.0) {
			double rotation_radians = info.rotation_degrees / 180.0 * std::numbers::pi;
			filter_string += std::format(
				"rotate="
					"a={0}:"
					"out_w=rotw({0}):"
					"out_h=roth({0})"
				",",

				rotation_radians
			);
		}

		filter_string += std::format(
				"scale={}:{}:force_original_aspect_ratio=decrease,"
				"pad={}:{}:-1:-1",
				output.width,
				output.height,
				output.width,
				output.height
		);

		if(output.fixed_fps) {
			filter_string += std::format(",fps={}", output.fps);
		}

		const util::SFrameInfo s_info{info};

		std::vector<std::unique_ptr<FrameSource>> sources;
		sources.push_back(std::move(src));

		return std::make_unique<FFMpegFilter>(span, std::move(sources), filter_string.c_str(), std::span(&s_info, 1), StreamType::Video);
	}

	Resampler::Resampler(Span span, std::unique_ptr<FrameSource>&& src, const util::AFrameInfo& info, const AudioParameters& output, std::optional<int64_t> out_duration)
	: m_span(span)
	, m_src(std::move(src))
	, m_input(nullptr)
	, m_output(nullptr)
	, m_filter_graph(nullptr)
	, m_sample_idx(std::numeric_limits<size_t>::max())
	, m_sample_rate(output.sample_rate)
	{
		AVSampleFormat out_sample_format = util::SampleFormat_get_AVSampleFormat(output.sample_format);
		bool do_resample = 
			info.sample_rate    != output.sample_rate ||
			info.sample_fmt     != out_sample_format  ||
			info.channel_layout != output.channel_layout;

		std::string filter_string;

		if(out_duration && do_resample) {
			// Do a quick & dirty trim first so that we don't have to resample as much audio
			const AVRational sample_dur = {1, info.sample_rate};
			const uint64_t end_sample = av_rescale_q_rnd(*out_duration, constants::TIMEBASE, sample_dur, AV_ROUND_UP);

			filter_string += std::format(
				"atrim=end_sample={},",
				end_sample
			);
		}
		
		if(do_resample) {
			filter_string += std::format(
				"aresample="
					"out_sample_rate={}:"
					"out_sample_fmt={}:"
					"out_chlayout={},"
				,
				output.sample_rate,
				(int) out_sample_format,
				output.channel_layout
			);
		}

		if(out_duration) {
			const AVRational sample_dur = {1, output.sample_rate};
			const uint64_t end_sample = av_rescale_q(*out_duration, constants::TIMEBASE, sample_dur);

			filter_string += std::format(
				"atrim=end_sample={},"
				"apad=whole_len={}",
				end_sample, end_sample
			);
		}

		if(!out_duration && !do_resample) {
			filter_string = "anull";
		}

		if(filter_string.ends_with(',')) {
			filter_string.pop_back();
		}

		const util::SFrameInfo s_info{info};

		util::FilterGraphInfo graph_info = util::create_filtergraph(m_span, filter_string.c_str(), std::span(&s_info, 1), StreamType::Audio);

		m_filter_graph = graph_info.graph;
		m_input = graph_info.inputs[0];
		m_output = graph_info.output;
	}

	bool Resampler::next_frame(AVFrame **p_frame) {
		m_sample_idx++;

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

		(*p_frame)->pts      = av_rescale_q(m_sample_idx * constants::SAMPLES_PER_FRAME, {1, static_cast<int>(m_sample_rate)}, constants::TIMEBASE);
		(*p_frame)->duration = av_rescale_q((*p_frame)->nb_samples,                      {1, static_cast<int>(m_sample_rate)}, constants::TIMEBASE);

		return true;
	}

	Resampler::~Resampler() {
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

	std::unique_ptr<PacketSource> encode(FilterContext& ctx, Span span, const VFilter& filter, StreamType type) {
		std::string hash;
		{
			Hasher hasher;

			hasher.add("_cached-stream_");

			const size_t start = hasher.pos();

			if(type == StreamType::Video) {
				ctx.vparams.hash(hasher);
			} else {
				ctx.aparams.hash(hasher);
			}

			filter.hash(hasher);

			hasher.add(static_cast<uint64_t>(hasher.pos() - start));

			hash = hasher.into_string();
		}

		try {
			std::filesystem::create_directory("./vcat-cache");
		} catch(const std::filesystem::filesystem_error& e) {
			throw error::failed_cache_directory(span, e.what());
		}

		std::string cached_name = format(
			"./vcat-cache/{}.{}",
			hash,
			StreamType_file_extension(type)
		);

		if(std::filesystem::exists(cached_name)) {
			return std::make_unique<VideoFilePktSource>(ctx, type, cached_name, span);
		}

		std::string tmp_cached_name = format(
			"./vcat-cache/~{}.{}",
			hash,
			StreamType_file_extension(type)
		);

		AVFormatContext *output = nullptr;
		error::handle_ffmpeg_error(span,
			avformat_alloc_output_context2(&output, nullptr, nullptr, tmp_cached_name.c_str())
		);

		AVStream *ostream = avformat_new_stream(output, nullptr);

		error::handle_ffmpeg_error(span,
			ostream ? 0 : AVERROR_UNKNOWN
		);

		AVCodecContext *encoder;
		if(type == StreamType::Video) {
			encoder = util::create_video_encoder(span, ctx.vparams);
		} else {
			encoder = util::create_audio_encoder(span, ctx.aparams);
		}

		error::handle_ffmpeg_error(span,
			avcodec_parameters_from_context(ostream->codecpar, encoder)
		);

		ostream->time_base = constants::TIMEBASE;

		if(type == StreamType::Audio) {
			ostream->time_base = AVRational {1, ctx.aparams.sample_rate};
		}

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

		std::unique_ptr<FrameSource> frames = filter.get_frames(ctx, type, span);

		AVBufferPool *buffer_pool = av_buffer_pool_init(sizeof(int64_t), nullptr);

		int res;
		while((res = avcodec_receive_packet(encoder, packet)) != AVERROR_EOF) {
			if(res == AVERROR(EAGAIN)) {
				if(frames->next_frame(&frame)) {
					assert(frame->duration);

					if(type == StreamType::Video) {
						frame->opaque_ref = av_buffer_pool_get(buffer_pool);
						*reinterpret_cast<int64_t *>(frame->opaque_ref->data) = frame->duration;
					}

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

			if(packet->pts >= 0) { // Don't write audio priming packet
				av_packet_rescale_ts(packet, encoder->time_base, ostream->time_base);

				av_write_frame(output, packet);
			}

			// The x264 encoder will not fill in the packet duration
			// We must do that ourselves
			if(packet->opaque_ref && !packet->duration) {
				packet->duration = *reinterpret_cast<int64_t *>(packet->opaque_ref->data);
			}

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

		return std::make_unique<VideoFilePktSource>(ctx, type, cached_name, span);
	}

	std::unique_ptr<PacketSource> VFilter::get_pkts(FilterContext& ctx, StreamType type, Span span) const {
		return encode(ctx, span, *this, type);
	}

	AVMediaType StreamType_to_AVMediaType(StreamType type) {
		switch(type) {
			case StreamType::Video:
				return AVMEDIA_TYPE_VIDEO;
			case StreamType::Audio:
				return AVMEDIA_TYPE_AUDIO;
		}

		std::cerr << "internal error: invalid StreamType `"<<(int) type<<"`\n";
		std::abort();
	}

	std::string_view StreamType_file_extension(StreamType type) {
		switch(type) {
			case StreamType::Video:
				return "mp4";
			case StreamType::Audio:
				return "m4a";
		}

		std::cerr << "internal error: invalid StreamType `"<<(int) type<<"`\n";
		std::abort();
	}
}
