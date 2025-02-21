#pragma once

#include "src/filter/filter.hh"
#include "src/util.hh"

namespace vcat::filter {
	class VideoFile : public VFilter {
			inline VideoFile() {};
		public:
			void hash(vcat::Hasher& hasher) const;
			std::string to_string() const;
			std::string type_name() const;

			std::unique_ptr<FrameSource> get_frames(FilterContext&, StreamType, Span) const;

			// NOTE: throws `std::string` upon IO failure
			VideoFile(std::string&& path);

		private:
			std::array<uint8_t, vcat::Hasher::HASH_SIZE> m_file_hash;
			std::string m_path;
	};
	static_assert(!std::is_abstract<VideoFile>());
};
