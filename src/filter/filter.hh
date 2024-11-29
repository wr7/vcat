#pragma once

#include "src/eval/eobject.hh"
#include <memory>

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
			virtual std::unique_ptr<PacketSource> get_pkts(Span) const = 0;
	};
	static_assert(std::is_abstract<VFilter>());

	class VideoFile : public VFilter {
		public:
			VideoFile() = delete;
			void hash(dvel::Hasher& hasher) const;
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

}
