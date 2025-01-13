#pragma once

#include "src/filter/filter.hh"

namespace vcat::filter {
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
};
