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
#elif defined(__WINDOWS__)
#  include <winsock2.h>
#  include <sys/param.h>
#  define be16toh(x) ntohs(x)
#  define be32toh(x) ntohl(x)
#  define be64toh(x) ntohll(x)
#  define htobe16(x) htons(x)
#  define htobe32(x) htonl(x)
#  define htobe64(x) htonll(x)
#else
#error "Unsupported platform"
#endif

extern "C" {
	#include "libavutil/hash.h"
}

namespace vcat {
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

	void Hasher::add(uint8_t data) {
		Hasher::add(&data, sizeof(data));
	}

	void Hasher::add(uint16_t data) {
		data = htobe16(data);

		Hasher::add(&data, sizeof(data));
	}

	void Hasher::add(uint32_t data) {
		data = htobe32(data);

		Hasher::add(&data, sizeof(data));
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
		av_hash_update(m_hasher, (const uint8_t *) data, nbytes);
	}
}
