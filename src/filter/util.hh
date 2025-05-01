#pragma once

#include "src/error.hh"
#include "src/filter/params.hh"
#include "src/util.hh"
#include <variant>

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

	AVCodecContext *create_video_encoder(Span span, const VideoParameters& params);
	AVCodecContext *create_audio_encoder(Span span, const AudioParameters& params);

	struct VFrameInfo {
		VFrameInfo(const AVCodecParameters*);

		int                           width;
		int                           height;
		AVPixelFormat                 pix_fmt;
		AVColorSpace                  color_space;
		AVColorRange                  color_range;
		AVColorPrimaries              color_primaries;
		AVColorTransferCharacteristic color_trc;
		AVRational                    sar;

		// Indicates how the video must be rotated when rendered.
		//
		// Many phones will record all videos in landscape mode but will include a special tag for
		// portrait mode which will cause video players to play the video in the correct rotation).
		double                        rotation_degrees;
	};

	struct AFrameInfo {
		AFrameInfo(const AVCodecParameters*, Span);

		int            sample_rate;
		AVSampleFormat sample_fmt;
		uint64_t       channel_layout; // From AV_CH_LAYOUT_* macros in libavutil/channel_layout.h
	};

	using SFrameInfo = std::variant<VFrameInfo, AFrameInfo>;

	struct FilterGraphInfo {
		AVFilterGraph *graph;
		std::vector<AVFilterContext *> inputs;
		AVFilterContext *output;
	};

	enum class StreamType {
		Video = 0,
		Audio = 1,
	};

	FilterGraphInfo create_filtergraph(Span span, const char *string, std::span<const SFrameInfo> input_info, StreamType output_type);

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

	/// `avfilter_get_by_name` but with proper exception error handling
	const AVFilter *get_avfilter(const char *name, Span);

	AVSampleFormat SampleFormat_get_AVSampleFormat(shared::SampleFormat format);
}
