#pragma once

#include "src/util.hh"

#include <string>
#include <type_traits>

namespace dvel {
	class EObject {
		virtual void hash(dvel::Hasher& hash) const = 0;
		virtual std::string to_string() const = 0;
	};
	static_assert(std::is_abstract<EObject>());

	class VideoFile : EObject {
		public:
			VideoFile() = delete;
			void hash(dvel::Hasher& hash) const;
			std::string to_string() const;

			VideoFile(std::string&& path);

		private:
			uint8_t m_file_hash[32];
			std::string m_path;
	};
	static_assert(!std::is_abstract<VideoFile>());
}

