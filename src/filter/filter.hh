#pragma once

#include "src/error.hh"
#include "src/eval/eobject.hh"
#include "src/filter/error.hh"
#include "src/filter/params.hh"
#include <memory>
#include <vector>

extern "C" {
	#include <libavcodec/packet.h>
	#include <libavcodec/codec_par.h>
	#include <libavformat/avformat.h>
	#include <libavutil/frame.h>
}

namespace vcat::filter {
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
			virtual TsInfo  dts_end_info() const = 0;
			virtual int64_t dts_start() const = 0;
			virtual TsInfo  pts_end_info() const = 0;
			virtual ~PacketSource() = default;
	};

	class VFilter : public EObject {
		public:
			virtual std::unique_ptr<PacketSource> get_pkts(Span, const VideoParameters&) const;
			virtual std::unique_ptr<FrameSource>  get_frames(Span, const VideoParameters&) const = 0;
	};
	static_assert(std::is_abstract<VFilter>());

	class VideoFile : public VFilter {
			inline VideoFile() {};
		public:
			void hash(vcat::Hasher& hasher) const;
			std::string to_string() const;
			std::string type_name() const;

			std::unique_ptr<FrameSource> get_frames(Span, const VideoParameters&) const;

			// NOTE: throws `std::string` upon IO failure
			VideoFile(std::string&& path);

		private:
			uint8_t m_file_hash[32];
			std::string m_path;
	};
	static_assert(!std::is_abstract<VideoFile>());

	class Concat : public VFilter {
		public:
			Concat() = delete;
			void hash(vcat::Hasher& hasher) const;
			std::string to_string() const;
			std::string type_name() const;

			std::unique_ptr<PacketSource> get_pkts(Span, const VideoParameters& params) const;
			std::unique_ptr<FrameSource>  get_frames(Span, const VideoParameters&) const;

			constexpr Concat(std::vector<Spanned<const VFilter&>> videos, Span s) : m_videos(std::move(videos)) {
				if(m_videos.empty()) {
					throw error::expected_video_got_none(s);
				}
			}
		private:
			std::vector<Spanned<const VFilter&>> m_videos;
	};
	static_assert(!std::is_abstract<Concat>());

}
