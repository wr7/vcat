#pragma once

#include "src/error.hh"
#include "src/filter/params.hh"
#include "src/util.hh"

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavcodec/codec_par.h>
	#include <libavfilter/avfilter.h>
	#include <libavformat/avformat.h>
	#include <libavformat/avio.h>
	#include <libavutil/frame.h>
	#include <libavutil/rational.h>
}

namespace vcat::filter::util {
	bool codecs_are_compatible(const AVCodecParameters *params1, const AVCodecParameters *params2);
	AVCodecContext *create_decoder(Span span, const AVCodecParameters *params);
	AVCodecContext *create_encoder(Span span, const VideoParameters& params);

	struct FrameInfo {
		FrameInfo(const AVCodecContext *);

		int           width;
		int           height;
		AVPixelFormat pix_fmt;
		AVRational    sar;
	};

	AVFilterGraph  *create_filtergraph(Span span, const char *string, const FrameInfo& input_info, AVFilterContext **input, AVFilterContext **output);

	void hash_avcodec_params(Hasher& hasher, const AVCodecParameters& p, Span s);

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

	class Rescaler {
		public:
			Rescaler() = delete;
			Rescaler(Span span, const FrameInfo& info, const VideoParameters& output);

			void rescale(AVFrame *frame) const;

			~Rescaler();
		private:
			Span           m_span;

			AVFilterContext *m_input;
			AVFilterContext *m_output;

			AVFilterGraph *m_filter_graph;
	};
}
