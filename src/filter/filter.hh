#pragma once

#include "src/error.hh"
#include "src/eval/eobject.hh"
#include "src/filter/params.hh"
#include "src/filter/util.hh"
#include <memory>
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

			constexpr FilterContext(VideoParameters&& vparams)
				: vparams(std::move(vparams))
			{}

			VideoParameters vparams;
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

	class VFilter : public EObject {
		public:
			virtual std::unique_ptr<PacketSource> get_pkts(FilterContext&, Span) const;
			virtual std::unique_ptr<FrameSource>  get_frames(Span, const VideoParameters&) const = 0;
	};
	static_assert(std::is_abstract<VFilter>());

	std::unique_ptr<PacketSource> encode(FilterContext& ctx, Span span, const VFilter& filter);

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
			int64_t          m_video_idx;
			size_t           m_pkt_no; //< The index of the current packet (starting at 0)

			std::vector<PacketTimestampInfo> m_ts_info;

			util::VCatAVFile         m_file;
		public:
			VideoFilePktSource() = delete;

			VideoFilePktSource(const std::string& path, Span span);
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
			void calculate_info(const std::string& path);
	};

	class Decode : public FrameSource {
		public:
			Decode(Span s, std::unique_ptr<PacketSource>&& packet_src, const VideoParameters& video_params);
			bool next_frame(AVFrame **frame);
			~Decode();

		private:
			Span                          m_span;
			std::unique_ptr<PacketSource> m_packets;
			AVCodecContext               *m_decoder;
			util::Rescaler                m_rescaler;
			AVPacket                     *m_pkt_buf;
	};
}
