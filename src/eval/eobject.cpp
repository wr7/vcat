#include "src/eval/eobject.hh"
#include "sha256.h"
#include "src/util.hh"
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <format>
#include <fstream>
#include <ios>
#include <sstream>
#include <string>

namespace dvel {
	void VideoFile::hash(Hasher& hasher) const {
		hasher.add("videofile_");
		hasher.add(&m_file_hash, sizeof(m_file_hash));
		hasher.add((uint64_t) 32);
	}

	// NOTE: throws `std::string` upon IO failure
	VideoFile::VideoFile(std::string&& path) : m_path(std::move(path)) {
		std::ifstream f;
		f.open(m_path, std::ios_base::in | std::ios_base::binary);

		char buf[512];

		f.rdbuf()->pubsetbuf(0, 0);

		SHA256 hasher;

		while(f.good()) {
			size_t num_bytes = f.read(&buf[0], sizeof(buf)).gcount();

			hasher.add(&buf[0], num_bytes);
		}

		hasher.getHash(m_file_hash);

		if(f.fail()) {
			throw std::format("Failed to open file `{}`: {}", path, strerror(errno));
		}

		f.close();
	}

	std::string VideoFile::to_string() const {
		std::stringstream s;
		s << "VideoFile(\""
			<< PartiallyEscaped(m_path)
			<< "\")";

		return s.str();
	}
}
