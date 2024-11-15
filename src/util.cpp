#include "src/util.hh"
#include <cstdint>
#include <string>

// 😭
#if defined(__linux__) && !defined (__ANDROID__)
#  include <endian.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__)
#  include <sys/endian.h>
#elif defined(__OpenBSD__) || defined(__ANDROID__)
#  include <sys/types.h>
#  define be16toh(x) betoh16(x)
#  define be32toh(x) betoh32(x)
#  define be64toh(x) betoh64(x)
#endif

namespace dvel {
	std::string indent(std::string&& input, size_t level) {
		for(size_t i = input.length() - 1;;) {
			if(input[i] == '\n') {
				for(size_t j = 0; j < level; j++) {
					input.insert(i + 1, "  ");
				}
			}

			if(i == 0) {
				break;
			} else {
				i--;
			}
		}

		if(!input.empty()) {
			for(size_t i = 0; i < level; i++) {
				input.insert(0, "  ");
			}
		}

		return std::move(input);
	}

	void Hasher::add(uint64_t data) {
		data = htobe64(data);

		Hasher::add(&data, sizeof(data));
	}

	void Hasher::add(std::string_view string) {
		Hasher::add(string.data(), string.size());
	}

	void Hasher::add(const void *data, const size_t nbytes) {
		m_nbytes += nbytes;
		m_hasher.add(data, nbytes);
	}
}
