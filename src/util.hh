#pragma once

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <array>
#include <utility>

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
			void add(uint8_t data);
			void add(uint16_t data);
			void add(uint32_t data);
			void add(uint64_t data);

			inline void add(int8_t data)  {add(std::bit_cast<uint8_t >(data));}
			inline void add(int16_t data) {add(std::bit_cast<uint16_t>(data));}
			inline void add(int32_t data) {add(std::bit_cast<uint32_t>(data));}
			inline void add(int64_t data) {add(std::bit_cast<uint64_t>(data));}

			inline std::array<uint8_t, 32> into_bin() {
				std::array<uint8_t, 32> retval = {0};

				av_hash_final_bin(m_hasher, &retval[0], 32);

				return retval;
			}

			// Gets the hash as a BASE64 string.
			//
			// This will consume the underlying object. It is undefined behavior to do anything with this
			// object after this method has been called.
			inline std::string into_string() {
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

	class HexDump {
		public:
			constexpr HexDump(std::span<uint8_t> data)
				: m_data(data) {}

			HexDump() = delete;
			friend std::ostream& operator<<(std::ostream& out, const HexDump& h);

		private:
			std::span<uint8_t> m_data;
	};

	template<size_t lhs_len, size_t rhs_len>
	consteval StringLiteral<lhs_len + rhs_len - 1> operator+(const char(&lhs)[lhs_len], StringLiteral<rhs_len> rhs) {
		return StringLiteral(lhs) + rhs;
	}

	template <typename T>
	class Reversed {
		private:
			T m_inner;
		public:
			inline Reversed(T iter)
				: m_inner(iter) {}

			inline decltype(m_inner.next()) next_back() {
				return m_inner.next();
			}
			inline decltype(m_inner.next_back()) next() {
				return m_inner.next_back();
			}
	};

	/// A possibly uninitialized value of a class.
	///
	/// The destructor must be explicitly called.
	template<std::movable T>
	class MaybeUninit
	{
		public:
			/// Stores a value into this object.
			constexpr void write(T&& val) {
				new(&m_inner) T(std::move(val));
			}

			constexpr MaybeUninit(T&& val) {
				this->write(val);
			}

			constexpr MaybeUninit() {};
			constexpr ~MaybeUninit() {};

			constexpr const T *operator->() const {
				return &m_inner;
			}

			constexpr T *operator->() {
				return &m_inner;
			}

			constexpr const T& operator*() const {
				return m_inner;
			}

			constexpr T& operator*() {
				return m_inner;
			}
		private:
			union {
				T m_inner;
			};
	};
}
