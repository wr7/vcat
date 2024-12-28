#include "src/util.hh"
#include "src/error.hh"
#include "src/filter/error.hh"
#include "src/filter/util.hh"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <endian.h>
#include <iostream>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavcodec/codec_par.h>
	#include <libavcodec/packet.h>
	#include <libavcodec/codec_id.h>
	#include <libavformat/avformat.h>
	#include <libavformat/avio.h>
	#include <libavutil/avutil.h>
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

	AVCodecContext *create_decoder(Span span, const AVCodecParameters *params, AVRational) {
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

	// Converts Annex-B bitstream to AVCC bitstream
	void h264_annexb_to_avcc(AVPacket *in, AVPacket **out, uint8_t nalu) {
		assert(out);
		assert(1 <= nalu && nalu <= 4);

		if(!*out) {
			*out = av_packet_alloc();
		}

		av_new_packet(*out, in->size);
		av_packet_copy_props(*out, in);

		size_t out_idx = 0;
		size_t nal_start = 0; // the start of the packet including the length prefix

		for(int i = 0; i < in->size; i++) {
			if(in->data[i] == 0x00 &&
			   in->size - i >= 4 &&
			   in->data[i + 1] == 0x00 &&
			   in->data[i + 2] == 0x01
			){
				if(out_idx > 0 && (*out)->data[out_idx - 1] == 0x00) {
					out_idx -= 1;
				}

				if(out_idx != 0) {
					size_t nal_len = out_idx - nal_start - nalu;
					nal_len = htobe32(nal_len << (32 - 8 * nalu));

					memcpy((*out)->data + nal_start, &nal_len, nalu);
				}

				nal_start = out_idx;
				out_idx += nalu;

				i += 2;
			} else {
				if(out_idx >= (size_t) (*out)->size) {
					av_grow_packet(*out, 5);
				}

				(*out)->data[out_idx++] = in->data[i];
			}
		}

		if(out_idx != 0) {
			size_t nal_len = out_idx - nal_start - nalu;
			nal_len = htobe32(nal_len << (32 - 8 * nalu));

			memcpy((*out)->data + nal_start, &nal_len, nalu);
		}

		av_shrink_packet(*out, out_idx);
	}
}
