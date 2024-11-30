#pragma once

#include "src/error.hh"
#include "src/eval/eobject.hh"
#include "src/filter/error.hh"
#include <iterator>
#include <memory>
#include <vector>

extern "C" {
	#include <libavcodec/packet.h>
	#include <libavformat/avformat.h>
}

namespace dvel::filter {
	class PacketSource {
		public:
			// Gets the next packet or returns false if `EOF` is reached
			virtual bool next_pkt(AVPacket *packet) = 0;
			virtual std::vector<AVStream *> streams() = 0;
	};

	class VFilter : public EObject {
		public:
			virtual std::unique_ptr<VFilter> clone() const = 0;
			virtual std::unique_ptr<PacketSource> get_pkts(Span) const = 0;
	};
	static_assert(std::is_abstract<VFilter>());

	class VideoFile : public VFilter {
			inline VideoFile() {};
		public:
			void hash(dvel::Hasher& hasher) const;
			std::string to_string() const;
			std::string type_name() const;

			std::unique_ptr<PacketSource> get_pkts(Span) const;

			// NOTE: throws `std::string` upon IO failure
			VideoFile(std::string&& path);

			inline std::unique_ptr<VFilter> clone() const {
				std::unique_ptr<VideoFile> clone = std::unique_ptr<VideoFile>(new VideoFile());
				clone->m_path = m_path;
				std::copy(std::begin(m_file_hash), std::end(m_file_hash), clone->m_file_hash);

				return clone;
			}

		private:
			uint8_t m_file_hash[32];
			std::string m_path;
	};
	static_assert(!std::is_abstract<VideoFile>());

	class Concat : public VFilter {
		public:
			Concat() = delete;
			void hash(dvel::Hasher& hasher) const;
			std::string to_string() const;
			std::string type_name() const;

			std::unique_ptr<PacketSource> get_pkts(Span) const;

			constexpr Concat(std::vector<Spanned<std::unique_ptr<VFilter>>> videos, Span s) : m_videos(std::move(videos)) {
				if(m_videos.empty()) {
					throw error::expected_video_got_none(s);
				}
			}

			inline std::unique_ptr<VFilter> clone() const {
				std::vector<Spanned<std::unique_ptr<VFilter>>> videos;
				videos.reserve(m_videos.size());

				for(const auto& video : m_videos) {
					videos.push_back(Spanned(video->get()->clone(), video.span));
				}

				return std::make_unique<Concat>(std::move(videos), Span(0, 0));
			}
		private:
			std::vector<Spanned<std::unique_ptr<VFilter>>> m_videos;
	};
	static_assert(!std::is_abstract<Concat>());

}
