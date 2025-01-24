#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "src/util/base32.hh"
#include "src/util.hh"

namespace vcat {
	constexpr std::string_view base32_lut = "0123456789abcdefghjkmnpqrstvwxyz";

	class Base32State {
		private:
			std::span<const uint8_t> m_input;
			size_t m_bit_idx;

		public:

			Base32State(std::span<const uint8_t> input)
			: m_input(input)
			, m_bit_idx(0) {}

			char next() {
				const size_t byte_idx = m_bit_idx / 8;
				const size_t  bit_idx = m_bit_idx % 8;

				if(byte_idx >= m_input.size()) {
					return 0;
				}

				m_bit_idx += 5;

				uint8_t digit = shr(shl(m_input[byte_idx], bit_idx), 3);

				if(8 - bit_idx >= 5 || byte_idx + 1 >= m_input.size()) {
					return base32_lut[digit];
				}

				digit |= shr(m_input[byte_idx + 1], 11 - bit_idx);

				return base32_lut[digit];
			}
	};

	std::string base32_encode(std::span<const uint8_t> input) {
		std::string retval;

		char c;
		for(Base32State state{input}; (c = state.next());) {
			retval.push_back(c);
		}

		return retval;
	}
}
