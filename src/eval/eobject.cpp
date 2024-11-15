#include "src/eval/eobject.hh"
#include "sha256.h"
#include "src/util.hh"
#include <cstdint>
#include <fstream>
#include <ios>
#include <string>

namespace dvel {
	void VideoFile::hash(Hasher& hasher) const {
		hasher.add("videofile_");
		hasher.add(&m_file_hash, sizeof(m_file_hash));
		hasher.add((uint64_t) 32);
	}

	VideoFile::VideoFile(std::string&& path) : m_path(std::move(path)) {
		std::ifstream f;
		f.open(m_path, std::ios_base::in | std::ios_base::binary);

		char buf[512];

		f.rdbuf()->pubsetbuf(0, 0);

		SHA256 hasher;

		while(f.good()) {
			size_t num_bytes = f.read(&buf[0], 512).gcount();

			hasher.add(&buf[0], num_bytes);
		}

		hasher.getHash(m_file_hash);

		if(f.fail()) {
			throw errno; // 😭
		}

		f.close();
	}
}
