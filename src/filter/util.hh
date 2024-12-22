#pragma once

#include "src/error.hh"
#include "src/util.hh"

extern "C" {
	#include <libavcodec/avcodec.h>
	#include "libavcodec/codec_par.h"
	#include <libavformat/avformat.h>
	#include <libavformat/avio.h>
}

namespace vcat::filter::util {
	bool codecs_are_compatible(const AVCodecParameters *params1, const AVCodecParameters *params2);
	AVCodecContext *create_decoder(Span span, const AVCodecParameters *params, AVRational time_base);
	AVCodecContext *create_encoder(Span span, const AVCodecParameters *params, AVRational time_base);

	void hash_avcodec_params(Hasher& hasher, const AVCodecParameters& p, Span s);

	// Convenience function for receiving packets through a decoder + encoder chain.
	//
	// The return value is the same as the `avcodec_receive_*` functions
	//
	// If a pointer to `NULL` is passed in to `frame_buf`, a new `AVFrame` will be allocated.
	int transcode_receive_packet(AVCodecContext *decoder, AVCodecContext *encoder, AVFrame **frame_buf, AVPacket *packet);

	// Gets the next packet in a stream. Returns `false` if the stream is empty.
	bool read_packet_from_stream(Span span, AVFormatContext *ctx, int stream_idx, AVPacket *packet);

	// Represents an `AVIOContext` based on `FILE *`.
	class VCatAVFile {
		public:
			constexpr AVIOContext *get() const {return m_ctx;}

			// Recreates the underlying `AVIOContext`.
			//
			// NOTE: this will invalidate all `VCatAVFile::get` pointers obtained before this call.
			void reset();

			// NOTE: this will take ownership of the file. When the destructor of this class is called, the
			// file will be closed.
			VCatAVFile(FILE *);

			constexpr VCatAVFile()
			 : m_ctx(nullptr) {}

			VCatAVFile(VCatAVFile&) = delete;
			VCatAVFile(VCatAVFile&&);
			~VCatAVFile();
		private:
			AVIOContext *m_ctx;
	};
}
