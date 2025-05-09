#pragma once

#include "src/error.hh"
#include "src/eval/eobject.hh"
#include "src/filter/params.hh"
#include "src/filter/util.hh"
#include "src/util.hh"
#include <cstdint>
#include <memory>
#include <ranges>
#include <vector>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavcodec/packet.h>
	#include <libavcodec/codec_par.h>
	#include <libavformat/avformat.h>
	#include <libavutil/frame.h>
}

namespace vcat::filter {
	class FilterContext {
		public:
			FilterContext(FilterContext&) = delete;
			FilterContext(FilterContext&&) = delete;

			constexpr FilterContext(VideoParameters&& vparams, AudioParameters&& aparams)
				: vparams(std::move(vparams))
				, aparams(std::move(aparams))
			{}

			VideoParameters vparams;
			AudioParameters aparams;
	};

	struct TsInfo {
		int64_t ts;
		int64_t duration;
	};

	class FrameSource {
		public:
			// Gets the next frame or returns false if `EOF` is reached
			//
			// A double pointer is used to allow for more efficient 'look-ahead' buffers for filters
			virtual bool next_frame(AVFrame **frame) = 0;
			virtual ~FrameSource() = default;
	};

	class PacketSource {
		public:
			// Gets the next packet or returns false if `EOF` is reached
			//
			// A double pointer is used to allow for more efficient 'look-ahead' buffers for filters
			virtual bool next_pkt(AVPacket **packet) = 0;
			virtual const AVCodecParameters *video_codec() = 0;
			virtual int64_t first_pkt_duration() const = 0;
			virtual size_t  dts_shift() const = 0;
			virtual TsInfo  pts_end_info() const = 0;
			virtual ~PacketSource() = default;
	};

	using StreamType = util::StreamType;

	// We are using regular functions because, for some reason, C++ enums cannot have methods

	AVMediaType StreamType_to_AVMediaType(StreamType);
	std::string_view StreamType_file_extension(StreamType);

	class VFilter : public EObject {
		public:
			virtual std::unique_ptr<PacketSource> get_pkts(FilterContext&, StreamType, Span) const;
			virtual std::unique_ptr<FrameSource>  get_frames(FilterContext&, StreamType, Span) const = 0;
	};
	static_assert(std::is_abstract<VFilter>());

	std::unique_ptr<PacketSource> encode(FilterContext& ctx, Span span, const VFilter& filter, StreamType);

	struct PacketTimestampInfo {
		int64_t pts;
		int64_t dts;
		int64_t duration;
		size_t  decode_idx;
	};

	class VideoFilePktSource : public PacketSource {
		private:
			AVFormatContext *m_ctx;
			Span             m_span;
			size_t           m_dts_shift;
			int64_t          m_stream_idx;
			size_t           m_pkt_no; //< The index of the current packet (starting at 0)

			std::vector<PacketTimestampInfo> m_ts_info;

			util::VCatAVFile         m_file;
		public:
			VideoFilePktSource() = delete;

			VideoFilePktSource(FilterContext& fctx, StreamType, const std::string& path, Span span);
			bool next_pkt(AVPacket **p_packet);
			const AVCodecParameters *video_codec();
			std::span<AVStream *> av_streams();
			int64_t first_pkt_duration() const;
			size_t  dts_shift() const;
			TsInfo  pts_end_info() const;

			~VideoFilePktSource();

			VideoFilePktSource(VideoFilePktSource&& old);

		private:
			// Walks through the file to calculate `m_dts_shift`, `m_pts_end_info`, and `video_idx`
			//
			// NOTE: this should be called before the main AVFormatContext is created.
			void calculate_info(FilterContext& ctx, StreamType, const std::string& path);
	};

	class FFMpegFilter : public FrameSource {
		public:
			FFMpegFilter() = delete;
			FFMpegFilter(Span span, std::vector<std::unique_ptr<FrameSource>>&& src, const char *filter, std::span<const util::SFrameInfo> input_info, StreamType output_type);

			inline FFMpegFilter(Span span, std::vector<std::unique_ptr<FrameSource>>&& src, const char *filter, const FilterContext& ctx, std::span<const StreamType> input_type, StreamType output_type)
				: FFMpegFilter(
					span,
					std::move(src),
					filter,
					input_type
						| std::ranges::views::transform([&](const StreamType& ty) {
							return ty == StreamType::Video ? util::SFrameInfo(ctx.vparams) : util::SFrameInfo(ctx.aparams);
						})
						| vcat::collect(),
					output_type
				) {}

			~FFMpegFilter();

			bool next_frame(AVFrame **frame);
		private:
			Span                                      m_span;
			std::vector<std::unique_ptr<FrameSource>> m_src;

			AVFilterGraph                            *m_filter_graph;
			std::vector<AVFilterContext *>            m_inputs;
			AVFilterContext                          *m_output;
	};

	static_assert(!std::is_abstract<FFMpegFilter>());

	class Decode : public FrameSource {
		public:
			Decode(Span s, std::unique_ptr<PacketSource>&& packet_src);
			bool next_frame(AVFrame **frame);
			~Decode();

		private:
			Span                          m_span;
			std::unique_ptr<PacketSource> m_packets;
			AVCodecContext               *m_decoder;
			AVPacket                     *m_pkt_buf;
	};

	std::unique_ptr<FrameSource> rescale(Span span, std::unique_ptr<FrameSource>&& src, const util::VFrameInfo& info, const VideoParameters& output);
	std::unique_ptr<FrameSource> resample(Span span, std::unique_ptr<FrameSource>&& src, const util::AFrameInfo& info, const AudioParameters& output, std::optional<int64_t> out_duration);
}
