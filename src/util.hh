#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <array>

extern "C" {
	#include <libavutil/hash.h>
}

namespace vcat {
	template <typename T> using OptionalRef = std::optional<std::reference_wrapper<T>>;
	std::string indent(std::string&& input, size_t level = 1);

	class Hasher {
		public:
			Hasher() {
				av_hash_alloc(&m_hasher, "SHA256");
				av_hash_init(m_hasher);
			}

			constexpr size_t pos() const {return m_nbytes;}

			void add(const void*data, size_t nbytes);
			void add(std::string_view string);
			void add(uint64_t data);

			inline std::array<uint8_t, 32> into_bin() {
				std::array<uint8_t, 32> retval = {0};

				av_hash_final_bin(m_hasher, &retval[0], 32);

				return retval;
			}

			inline std::string as_string() {
				std::string retval;
				retval.resize(45);

				av_hash_final_b64(m_hasher, (uint8_t *) retval.data(), retval.size() + 1);

				retval.resize(43);
				for(auto& c : retval) {
					switch(c) {
						case '+': c = '-'; break;
						case '/': c = '_'; break;
					}
				}

				return retval;
			}

			inline ~Hasher() {
				av_hash_freep(&m_hasher);
			}
		private:
			AVHashContext *m_hasher;
			std::size_t m_nbytes = 0;
	};

	// String type for use as a template argument. Note: `len` includes the null byte.
	//
	// Also overloads the `+` operator for compile-time string concatenation
	template<size_t len>
	class StringLiteral {
		public:
			consteval StringLiteral() {
				for(size_t i = 0; i < len; i++) {
					m_data[i] = '\0';
				}
			}

			consteval StringLiteral(const char (&str)[len]) {
				std::copy_n(str, len, m_data);
			}

			consteval size_t size() const {
				assert(len >= 1);
				assert(m_data[len - 1] == '\0');

				return len - 1;
			}

			constexpr std::string_view operator*() const {
				assert(len >= 1);
				assert(m_data[len - 1] == '\0');

				return std::string_view(m_data, len - 1);
			}

			template<size_t rhs_len>
			consteval StringLiteral<len + rhs_len - 1> operator+(StringLiteral<rhs_len> rhs) {
				StringLiteral<len + rhs_len - 1> result;

				assert(len >= 1);
				assert(rhs_len >= 1);
				assert(m_data[len - 1] == '\0');
				assert(rhs.m_data[rhs_len - 1] == '\0');

				std::copy_n(m_data, len - 1, result.m_data);
				std::copy_n(rhs.m_data, rhs_len, result.m_data + len - 1);

				return result;
			}

			template<size_t rhs_len>
			consteval StringLiteral<len + rhs_len - 1> operator+(const char(&rhs)[rhs_len]) {
				return *this + StringLiteral<rhs_len>(rhs);
			}

			char m_data[len];
	};

	template<size_t lhs_len, size_t rhs_len>
	consteval StringLiteral<lhs_len + rhs_len - 1> operator+(const char(&lhs)[lhs_len], StringLiteral<rhs_len> rhs) {
		return StringLiteral(lhs) + rhs;
	}

	template <std::bidirectional_iterator T>
	class Reversed {
		private:
			T m_inner;
		public:
			using difference_type = std::iter_difference_t<T>;
			using value_type      = std::iter_value_t<T>;
			using reference       = std::iter_reference_t<T>;

			constexpr Reversed<T>() : m_inner() {}
			constexpr Reversed<T>(T inner) : m_inner(inner) {}

			inline decltype(*m_inner) operator*() const {
				return *m_inner;
			}

			inline bool operator==(const Reversed<T> rhs) const {
				return m_inner == rhs.m_inner;
			}

			inline Reversed<T>& operator++() {
				--m_inner;
				return *this;
			}
			inline Reversed<T>& operator--() {
				++m_inner;
				return *this;
			}

			inline Reversed<T> operator++(int) {
				return m_inner--;
			}
			inline Reversed<T> operator--(int) {
				return m_inner++;
			}
	};
}
