#include "src/util.hh"
#include "src/constants.hh"
#include "src/error.hh"
#include "src/filter/error.hh"
#include "src/filter/params.hh"
#include "src/filter/util.hh"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <endian.h>
#include <format>
#include <iostream>
#include <variant>
#include <vector>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavcodec/codec_par.h>
	#include <libavcodec/packet.h>
	#include <libavcodec/codec_id.h>
	#include <libavfilter/avfilter.h>
	#include <libavfilter/buffersrc.h>
	#include <libavfilter/buffersink.h>
	#include <libavformat/avformat.h>
	#include <libavformat/avio.h>
	#include <libavutil/avstring.h>
	#include <libavutil/avutil.h>
	#include <libavutil/channel_layout.h>
	#include <libavutil/display.h>
	#include <libavutil/error.h>
	#include <libavutil/frame.h>
	#include <libavutil/rational.h>
}

namespace vcat::filter::util {
	bool codecs_are_compatible(const AVCodecParameters *params1, const AVCodecParameters *params2) {
		if(
			params1->codec_type     != params2->codec_type     ||
			params1->codec_id       != params2->codec_id       ||
			params1->codec_tag      != params2->codec_tag      ||
			params1->format         != params2->format         ||
			params1->profile        != params2->profile        ||
			params1->level          != params2->level          ||
			params1->extradata_size != params2->extradata_size )
		{
			return false;
		}

		for(int i = 0; i < params1->extradata_size; i++) {
			if(params1->extradata[i] != params2->extradata[i]) {
				return false;
			}
		}

		if(params1->codec_type == AVMEDIA_TYPE_VIDEO) {
			return
				params1->width           == params2->width           &&
				params1->height          == params2->height          &&
				params1->field_order     == params2->field_order     &&
				params1->color_range     == params2->color_range     &&
				params1->color_primaries == params2->color_primaries &&
				params1->color_trc       == params2->color_trc       &&
				params1->color_space     == params2->color_space     &&
				params1->chroma_location == params2->chroma_location ;
		} else if(params1->codec_type == AVMEDIA_TYPE_AUDIO) {
			if(
				params1->sample_rate           != params2->sample_rate           ||
				params1->block_align           != params2->block_align           ||
				params1->frame_size            != params2->frame_size            ||
				params1->ch_layout.order       != params2->ch_layout.order       ||
				params1->ch_layout.nb_channels != params2->ch_layout.nb_channels )
			{
				return false;
			}

			switch(params1->ch_layout.order) {
				case AV_CHANNEL_ORDER_NATIVE:
				case AV_CHANNEL_ORDER_AMBISONIC:
					return params1->ch_layout.u.mask == params2->ch_layout.u.mask;
				case AV_CHANNEL_ORDER_CUSTOM:
					return false;
				case FF_CHANNEL_ORDER_NB:
				case AV_CHANNEL_ORDER_UNSPEC:
					return true;
			}
		} else {
			std::cerr << "codecs_are_compatible on non-audio/video stream";
			std::abort();
		}

		return true;
	}

	void hash_avcodec_params(Hasher& hasher, const AVCodecParameters& p, Span s) {
		hasher.add("_avcodec-params_");
		const size_t start = hasher.pos();

		if(p.codec_type == AVMEDIA_TYPE_AUDIO) {
			hasher.add('A');
		} else if(p.codec_type == AVMEDIA_TYPE_VIDEO) {
			hasher.add('V');
		} else {
			std::cerr << "cannot hash non-audio/video codec";
			std::abort();
		}

		const char *codec_name = avcodec_get_name(p.codec_id);
		size_t      codec_name_len = strlen(codec_name);

		hasher.add(codec_name);
		hasher.add((uint64_t) codec_name_len);
		hasher.add(p.codec_tag);
		hasher.add(p.format);
		hasher.add(p.profile);
		hasher.add(p.level);
		hasher.add(p.extradata, p.extradata_size);
		hasher.add(p.extradata_size);

		if(p.codec_type == AVMEDIA_TYPE_VIDEO) {
			hasher.add(p.width);
			hasher.add(p.height);
			hasher.add(p.field_order);
			hasher.add(p.color_range);
			hasher.add(p.color_primaries);
			hasher.add(p.color_trc);
			hasher.add(p.color_space);
			hasher.add(p.chroma_location);
		} else {
			hasher.add(p.sample_rate);
			hasher.add(p.block_align);
			hasher.add(p.frame_size);
			switch(p.ch_layout.order) {
				case AV_CHANNEL_ORDER_NATIVE:
				case AV_CHANNEL_ORDER_AMBISONIC:
					hasher.add(p.ch_layout.u.mask); break;
				case AV_CHANNEL_ORDER_CUSTOM:
					throw error::unsupported_output_codec_params(s, "custom channel order");
				case FF_CHANNEL_ORDER_NB:
				case AV_CHANNEL_ORDER_UNSPEC:
					break;
			}
			hasher.add(p.ch_layout.order);
			hasher.add(p.ch_layout.nb_channels);
		}

		hasher.add((uint64_t) (hasher.pos() - start));
	}

	AVCodecContext *create_decoder(Span span, const AVCodecParameters *params) {
		const AVCodec *av_decoder = avcodec_find_decoder(params->codec_id);
		if(!av_decoder) {
			throw error::ffmpeg_no_codec(span, params->codec_id);
		}

		AVCodecContext *decode_ctx = avcodec_alloc_context3(av_decoder);
		error::handle_ffmpeg_error(span, decode_ctx ? 0 : AVERROR_UNKNOWN);

		error::handle_ffmpeg_error(span,
			avcodec_parameters_to_context(decode_ctx, params)
		);

		avcodec_open2(decode_ctx, av_decoder, nullptr);

		return decode_ctx;
	}

	AVCodecContext *create_video_encoder(Span span, const VideoParameters& params) {
		const AVCodec *av_encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
		if(!av_encoder) {
			throw error::ffmpeg_no_codec(span, AV_CODEC_ID_H264);
		}

		AVCodecContext *encode_ctx = avcodec_alloc_context3(av_encoder);
		error::handle_ffmpeg_error(span, encode_ctx ? 0 : AVERROR_UNKNOWN);

		encode_ctx->width = params.width;
		encode_ctx->height = params.height;
		encode_ctx->time_base = constants::TIMEBASE;
		encode_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

		encode_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER | AV_CODEC_FLAG_COPY_OPAQUE;

		avcodec_open2(encode_ctx, av_encoder, nullptr);

		return encode_ctx;
	}

	AVCodecContext *create_audio_encoder(Span span, const AudioParameters& params) {
		const AVCodec *aac_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
		if(!aac_codec) {
			throw error::ffmpeg_no_codec(span, AV_CODEC_ID_AAC);
		}

		AVCodecContext *encode_ctx = avcodec_alloc_context3(aac_codec);
		error::handle_ffmpeg_error(span, encode_ctx ? 0 : AVERROR_UNKNOWN);

		encode_ctx->sample_rate = params.sample_rate;
		encode_ctx->sample_fmt = SampleFormat_get_AVSampleFormat(params.sample_format);
		encode_ctx->time_base = constants::TIMEBASE; // TODO
		encode_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
		encode_ctx->profile = AV_PROFILE_AAC_LOW;
		encode_ctx->cutoff = 0;

		error::handle_ffmpeg_error(span,
			av_channel_layout_from_mask(&encode_ctx->ch_layout, constants::CHANNEL_LAYOUT)
		);

		encode_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

		avcodec_open2(encode_ctx, aac_codec, nullptr);

		return encode_ctx;
	}

	FilterGraphInfo create_filtergraph(Span span, const char *string, std::span<const SFrameInfo> input_info, StreamType output_type) {
		const AVFilter *const buffer = get_avfilter("buffer", span);
		const AVFilter *const buffersink = get_avfilter("buffersink", span);
		const AVFilter *const abuffer = get_avfilter("abuffer", span);
		const AVFilter *const abuffersink = get_avfilter("abuffersink", span);

		const AVFilter *const output_filter_type = output_type == StreamType::Video ? buffersink : abuffersink;

		AVFilterGraph *graph = avfilter_graph_alloc();
		error::handle_ffmpeg_error(span, graph ? 0 : AVERROR(ENOMEM));

		std::vector<AVFilterContext *> inputs;
		AVFilterContext *output = avfilter_graph_alloc_filter(graph, output_filter_type, "out");

		AVFilterInOut *output_inout = avfilter_inout_alloc();

		error::handle_ffmpeg_error(span, output && output_inout ? 0 : AVERROR(ENOMEM));

		error::handle_ffmpeg_error(span,
			avfilter_init_str(output, nullptr)
		);

		AVFilterInOut *input_inout = nullptr;
		AVFilterInOut *last_input_inout = nullptr;

		for(size_t i = 0; i < input_info.size(); i++) {
			const SFrameInfo& f_info = input_info[i];

			const AVFilter *input_filter_type;
			std::string input_filter_string;

			if(std::holds_alternative<VFrameInfo>(f_info)) {
				const VFrameInfo& v_info = std::get<0>(f_info);

				input_filter_type = buffer;

				input_filter_string = std::format(
						"width={}:height={}:pix_fmt={}:time_base={}/{}:colorspace={}:range={}:sar={}/{}",
						v_info.width,
						v_info.height,
						static_cast<int>(v_info.pix_fmt),
						constants::TIMEBASE.num,
						constants::TIMEBASE.den,
						static_cast<int>(v_info.color_space),
						static_cast<int>(v_info.color_range),
						v_info.sar.num,
						v_info.sar.den
				);
			} else {
				const AFrameInfo& a_info = std::get<1>(f_info);

				input_filter_type = abuffer;

				input_filter_string = std::format(
						"time_base={}/{}:sample_rate={}:sample_fmt={}:channel_layout={}",
						constants::TIMEBASE.num,
						constants::TIMEBASE.den,
						a_info.sample_rate,
						(int) a_info.sample_fmt,
						a_info.channel_layout
				);
			}

			char *filter_name = av_asprintf("in_%zu", i);

			AVFilterContext *input = avfilter_graph_alloc_filter(graph, input_filter_type, filter_name);
			AVFilterInOut *inout = avfilter_inout_alloc();

			error::handle_ffmpeg_error(span, input && inout ? 0 : AVERROR(ENOMEM));

			inputs.push_back(input);

			error::handle_ffmpeg_error(span,
				avfilter_init_str(input, input_filter_string.c_str())
			);

			inout->filter_ctx = input;
			inout->name = filter_name;
			inout->pad_idx = 0;
			inout->next = nullptr;

			if(last_input_inout) {
				last_input_inout->next = inout;
			} else {
				input_inout = inout;
			}

			last_input_inout = inout;
		}

		if(input_inout) {
			// Add an alias "in" for the first input.
			// We do this because if no input is specified in the filtergraph string, FFMPEG will implicitly use "in".
			AVFilterInOut *inout = avfilter_inout_alloc();
			error::handle_ffmpeg_error(span, inout ? 0 : AVERROR(ENOMEM));

			inout->filter_ctx = input_inout->filter_ctx;
			inout->name = av_strdup("in");
			inout->pad_idx = 0;
			inout->next = nullptr;

			last_input_inout->next = inout;
			last_input_inout = inout;
		}

		output_inout->filter_ctx = output;
		output_inout->name = av_strdup("out");
		output_inout->pad_idx = 0;
		output_inout->next = nullptr;

		error::handle_ffmpeg_error(span,
			avfilter_graph_parse(graph, string, output_inout, input_inout, nullptr)
		);

		error::handle_ffmpeg_error(span,
			avfilter_graph_config(graph, nullptr)
		);

		if(output_type == StreamType::Audio) {
			av_buffersink_set_frame_size(output, constants::SAMPLES_PER_FRAME);
		}

		return FilterGraphInfo {
			.graph = graph,
			.inputs = inputs,
			.output = output,
		};
	}

	bool read_packet_from_stream(Span span, AVFormatContext *ctx, int stream_idx, AVPacket *packet) {
		for(;;) {
			int ret = av_read_frame(ctx, packet);
			if(ret == AVERROR_EOF) {
				return false;
			}
			error::handle_ffmpeg_error(span, ret);

			if(packet->stream_index == stream_idx) {
				return true;
			}

			av_packet_unref(packet);
		}
	}

	static int read_packet(void *opaque, uint8_t *buf, int buf_size) {
		FILE *file = (FILE *) opaque;

		size_t a = fread(buf, 1, buf_size, file);

		if(a == 0) {
			if(feof(file)) {
				clearerr(file);
				return AVERROR_EOF;
			} else if(ferror(file)) {
				clearerr(file);
				return AVERROR(EIO);
			}
		}

		return a;
	}

	static int64_t seek_func(void *opaque, int64_t offset, int whence) {
		FILE *file = (FILE *) opaque;

		if (fseek(file, offset, whence) != 0) {
			return AVERROR(errno);
		}

		return ftell(file);
	}

	VCatAVFile::VCatAVFile(FILE *file) {
		assert(file);
		uint8_t *buffer = (decltype(buffer)) av_malloc(4096);
		m_ctx = avio_alloc_context(buffer, 4096, 0, file, read_packet, nullptr, seek_func);
	}

	void VCatAVFile::reset() {
		uint8_t *buffer = m_ctx->buffer;
		int buffer_size = m_ctx->buffer_size;

		m_ctx->buffer = nullptr;
		m_ctx->buffer_size = 0;

		FILE *file = (FILE *) m_ctx->opaque;
		assert(file);

		fseek(file, 0, SEEK_SET);

		avio_context_free(&m_ctx);
		m_ctx = avio_alloc_context(buffer, buffer_size, 0, file, read_packet, nullptr, seek_func);
	}

	VCatAVFile::VCatAVFile(VCatAVFile&& o)
		: m_ctx(o.m_ctx)
	{
		o.m_ctx = nullptr;
	}

	VCatAVFile::~VCatAVFile() {
		if(m_ctx) {
			assert(m_ctx->opaque);
			fclose((FILE *) m_ctx->opaque);
			av_free(m_ctx->buffer);
			avio_context_free(&m_ctx);
		}
	}

	VFrameInfo::VFrameInfo(const AVCodecParameters *params)
		: width(params->width)
		, height(params->height)
		, pix_fmt(static_cast<AVPixelFormat>(params->format))
		, color_space(params->color_space)
		, color_range(params->color_range)
		, color_primaries(params->color_primaries)
		, color_trc(params->color_trc)
		, sar(params->sample_aspect_ratio)
		, rotation_degrees(0.0)
	{
		assert(params->codec_type == AVMEDIA_TYPE_VIDEO);

		if(color_space == AVCOL_SPC_UNSPECIFIED) {
			color_space = constants::FALLBACK_COLOR_SPACE;
		}

		if(color_primaries == AVCOL_PRI_UNSPECIFIED) {
			color_primaries = constants::FALLBACK_COLOR_PRIMARIES;
		}

		if(color_trc == AVCOL_TRC_UNSPECIFIED) {
			color_trc = constants::FALLBACK_COLOR_TRANSFER_FUNCTION;
		}

		if(color_range == AVCOL_RANGE_UNSPECIFIED) {
			color_range = constants::FALLBACK_COLOR_RANGE;
		}


		for(int i = 0; i < params->nb_coded_side_data; i++) {
			if(params->coded_side_data[i].type != AV_PKT_DATA_DISPLAYMATRIX) {
				continue;
			}

			typedef int32_t DisplayMatrix[9];

			const DisplayMatrix *display_matrix = (DisplayMatrix *) params->coded_side_data[i].data;
			this->rotation_degrees = -av_display_rotation_get(*display_matrix);
			break;
		}
	}

	VFrameInfo::VFrameInfo(const VideoParameters& params)
		: width(params.width)
		, height(params.height)
		, pix_fmt(constants::PIXEL_FORMAT)
		, color_space(constants::COLOR_SPACE)
		, color_range(constants::COLOR_RANGE)
		, color_primaries(constants::COLOR_PRIMARIES)
		, color_trc(constants::COLOR_TRANSFER_FUNCTION)
		, sar(constants::SAMPLE_ASPECT_RATIO)
		, rotation_degrees(0.0)
	{}

	AFrameInfo::AFrameInfo(const AVCodecParameters *params, Span s)
		: sample_rate(params->sample_rate)
		, sample_fmt(static_cast<AVSampleFormat>(params->format))
	{
		assert(params->codec_type == AVMEDIA_TYPE_AUDIO);

		if(params->ch_layout.order == AV_CHANNEL_ORDER_NATIVE) {
			channel_layout = params->ch_layout.u.mask;
			return;
		} else if(params->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC) {
			AVChannelLayout layout;
			av_channel_layout_default(&layout, params->ch_layout.nb_channels);

			if(layout.order == AV_CHANNEL_ORDER_NATIVE) {
				channel_layout = layout.u.mask;
				return;
			}
		}

		throw error::unimplemented_channel_order(s);
	}
		// int            sample_rate;
		// AVSampleFormat sample_fmt;
		// uint64_t       channel_layout; // From AV_CH_LAYOUT_* macros in libavutil/channel_layout.h

	AFrameInfo::AFrameInfo(const AudioParameters& params)
		: sample_rate(params.sample_rate)
		, sample_fmt(SampleFormat_get_AVSampleFormat(params.sample_format))
		, channel_layout(params.channel_layout)
	{}

	const AVFilter *get_avfilter(const char *name, Span s) {
		const AVFilter *filter = avfilter_get_by_name(name);

		if(!filter) {
			throw error::ffmpeg_no_filter(s, name);
		}

		return filter;
	}

	AVSampleFormat SampleFormat_get_AVSampleFormat(shared::SampleFormat format) {
		switch(format) {
			case shared::s16:
				return AV_SAMPLE_FMT_S16P;
			case shared::s32:
				return AV_SAMPLE_FMT_S32P;
			case shared::flt:
				return AV_SAMPLE_FMT_FLTP;
		}

		std::cerr << "Internal error: invalid SampleFormat\n";
		std::abort();
	}
}
