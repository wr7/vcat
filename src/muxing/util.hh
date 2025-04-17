#pragma once

#include "src/filter/filter.hh"
#include <memory>

namespace vcat::muxing {
	class PacketInterleaver{
		public:
			PacketInterleaver(std::unique_ptr<filter::PacketSource>&& video, std::unique_ptr<filter::PacketSource>&& audio);

			// Gets the next packet. Returns `false` if there is no next packet.
			bool next(AVPacket **buffer);

			~PacketInterleaver();

			PacketInterleaver(PacketInterleaver&) = delete;
		private:
			std::unique_ptr<filter::PacketSource> m_video;
			std::unique_ptr<filter::PacketSource> m_audio;

			bool m_video_eof;
			AVPacket *m_video_buf;

			bool m_audio_eof;
			AVPacket *m_audio_buf;
	};
}
