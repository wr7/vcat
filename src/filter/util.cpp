#include "libavutil/rational.h"
#include "src/constants.hh"
#include "src/error.hh"
#include "src/filter/error.hh"
#include <cerrno>
#include <cstdlib>
#include <iostream>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavcodec/codec_par.h>
	#include <libavcodec/packet.h>
	#include <libavformat/avformat.h>
	#include <libavutil/avutil.h>
	#include <libavutil/error.h>
	#include <libavutil/frame.h>
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

	AVCodecContext *create_decoder(Span span, const AVCodecParameters *params, AVRational time_base) {
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

	AVCodecContext *create_encoder(Span span, const AVCodecParameters *params, AVRational time_base) {
		const AVCodec *av_encoder = avcodec_find_encoder(params->codec_id);
		if(!av_encoder) {
			throw error::ffmpeg_no_codec(span, params->codec_id);
		}

		AVCodecContext *encode_ctx = avcodec_alloc_context3(av_encoder);
		error::handle_ffmpeg_error(span, encode_ctx ? 0 : AVERROR_UNKNOWN);

		error::handle_ffmpeg_error(span,
			avcodec_parameters_to_context(encode_ctx, params)
		);

		encode_ctx->time_base = time_base;

		avcodec_open2(encode_ctx, av_encoder, nullptr);

		return encode_ctx;
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

	int transcode_receive_packet(AVCodecContext *decoder, AVCodecContext *encoder, AVFrame **frame_buf, AVPacket *packet) {
		if(!*frame_buf) {
			*frame_buf = av_frame_alloc();
			if(!frame_buf) {
				return AVERROR_UNKNOWN;
			}
		}

		AVFrame *frame = *frame_buf;

		for(;;) {
			int ret = avcodec_receive_packet(encoder, packet);
			if(ret == 0) {
				return 0;
			} else if(ret != AVERROR(EAGAIN)) {
				return ret;
			}

			ret = avcodec_receive_frame(decoder, frame);
			if(ret == AVERROR_EOF) {
				avcodec_send_frame(encoder, nullptr);
				continue;
			} else if(ret) {
				return ret;
			}

			ret = avcodec_send_frame(encoder, frame);
			assert(ret != AVERROR(EAGAIN));
			if(ret) {
				av_frame_unref(frame);
				return ret;
			}

			av_frame_unref(frame);
		}
	}
}
