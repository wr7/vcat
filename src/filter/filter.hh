#pragma once

#include "src/error.hh"
#include "src/eval/eobject.hh"
#include "src/filter/error.hh"
#include <memory>
#include <vector>

extern "C" {
	#include <libavcodec/packet.h>
	#include <libavformat/avformat.h>
}

namespace vcat::filter {
	class PacketSource {
		public:
			// Gets the next packet or returns false if `EOF` is reached
			//
			// A double pointer is used to allow for more efficient 'look-ahead' buffers for filters
			virtual bool next_pkt(AVPacket **packet) = 0;
			virtual std::span<AVStream *> streams() = 0;
	};

	class VFilter : public EObject {
		public:
			virtual std::unique_ptr<PacketSource> get_pkts(Span) const = 0;
	};
	static_assert(std::is_abstract<VFilter>());

	class VideoFile : public VFilter {
			inline VideoFile() {};
		public:
			void hash(vcat::Hasher& hasher) const;
			std::string to_string() const;
			std::string type_name() const;

			std::unique_ptr<PacketSource> get_pkts(Span) const;

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

			std::unique_ptr<PacketSource> get_pkts(Span) const;

			constexpr Concat(std::vector<Spanned<VFilter&>> videos, Span s) : m_videos(std::move(videos)) {
				if(m_videos.empty()) {
					throw error::expected_video_got_none(s);
				}
			}
		private:
			std::vector<Spanned<VFilter&>> m_videos;
	};
	static_assert(!std::is_abstract<Concat>());

}
