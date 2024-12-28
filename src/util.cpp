#include "src/util.hh"
#include <cstdint>
#include <iomanip>
#include <ios>
#include <ostream>
#include <span>
#include <string>

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

	std::ostream& operator<<(std::ostream& out, const HexDump& h) {
		std::ios prop(nullptr);
		prop.copyfmt(out);

		for(size_t i = 0; i < h.m_data.size(); i++) {
			if(i != 0) {
				out << " ";
			}

			out << std::setw(2) << std::setfill('0') << std::hex << +h.m_data[i];
		}

		out.copyfmt(prop);
		return out;
	}
}
