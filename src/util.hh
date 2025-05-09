#pragma once

#include <algorithm>
#include <bit>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <array>
#include <utility>
#include <vector>

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

#include "src/util/base32.hh"

extern "C" {
	#include <libavutil/hash.h>
	#include <libavutil/base64.h>
}

namespace vcat {
	template <typename T> using OptionalRef = std::optional<std::reference_wrapper<T>>;
	std::string indent(std::string&& input, size_t level = 1);

	class Hasher {
		private:
			static constexpr std::string HASH_NAME = "MD5";

		public:
			static constexpr size_t      HASH_SIZE = 16;

			Hasher() {
				av_hash_alloc(&m_hasher, HASH_NAME.c_str());
				av_hash_init(m_hasher);
			}

			constexpr size_t pos() const {return m_nbytes;}

			void add(const void*data, size_t nbytes);
			void add(std::string_view string);
			void add(uint8_t data);
			void add(uint16_t data);
			void add(uint32_t data);
			void add(uint64_t data);

			inline void add(bool data)    {add(std::bit_cast<uint8_t >(data));}
			inline void add(int8_t data)  {add(std::bit_cast<uint8_t >(data));}
			inline void add(int16_t data) {add(std::bit_cast<uint16_t>(data));}
			inline void add(int32_t data) {add(std::bit_cast<uint32_t>(data));}
			inline void add(int64_t data) {add(std::bit_cast<uint64_t>(data));}

			inline std::array<uint8_t, HASH_SIZE> into_bin() {
				std::array<uint8_t, HASH_SIZE> retval = {0};

				av_hash_final_bin(m_hasher, &retval[0], HASH_SIZE);

				return retval;
			}

			// Gets the hash as a Crockford base32 string.
			//
			// This will consume the underlying object. It is undefined behavior to do anything with this
			// object after this method has been called.
			inline std::string into_string() {
				return base32_encode(into_bin());
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
			constexpr HexDump(std::span<const uint8_t> data)
				: m_data(data) {}

			HexDump() = delete;
			friend std::ostream& operator<<(std::ostream& out, const HexDump& h);

		private:
			std::span<const uint8_t> m_data;
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

	/// Performs a binary search using a provided function.
	///
	/// The provided function should return an `std::strong_ordering` denoting whether the provided
	/// value is less than, equal to, or greater than the desired value.
	///
	/// If `true` is returned, the index of the element is returned.
	///
	/// Otherwise, the index of where the element should be inserted is returned.
	template<typename T, typename F>
	requires requires(F f, T& p, int& r) {
		{f(std::forward<T&>(p))} -> std::same_as<std::strong_ordering>;
	}
	std::pair<bool, size_t> binary_search_by(std::span<T> slice, F f) {
		if(slice.empty()) {
			return {false, 0};
		}

		size_t size = slice.size();
		size_t base = 0;

		while(size > 1) {
			const size_t mid = base + size / 2;

			auto res = f(std::forward<T&>(slice[mid]));

			base = res > 0 ? base : mid;

			size -= size/2;
		}

		auto res = f(std::forward<T&>(slice[base]));

		if(res == 0) {
			return {true, base};
		} else {
			return {false, base + (res < 0)};
		}
	}

	/// Performs a binary search.
	///
	/// If `true` is returned, the index of the element is returned.
	///
	/// Otherwise, the index of where the element should be inserted is returned.
	template<std::integral T>
	std::pair<bool, size_t> binary_search(std::span<T> slice, T val) {
		return binary_search_by(slice, [val](const auto& v){return v <=> val;});
	}

	template<std::unsigned_integral A, std::integral B>
	auto saturating_sub(A a, B b) {
		return (a >= b) ? (a - b) : 0;
	}

	// Like `a << b` except its return type is the same as `a`.
	template<std::unsigned_integral A, std::integral B>
	A shl(A a, B b) {
		return a << b;
	}

	// Like `a >> b` except its return type is the same as `a`.
	template<std::unsigned_integral A, std::integral B>
	A shr(A a, B b) {
		return a >> b;
	}

	struct collect_adapter {
		struct closure {
			template<std::ranges::range R>
			constexpr auto operator()(R&& r) const
			{
				auto r_common = r | std::views::common;

				return std::vector<std::ranges::range_value_t<R>>(r_common.begin(), r_common.end());
			}
		};

		constexpr closure operator()() const {
			return closure{};
		}


		template<std::ranges::range R>
		constexpr auto operator()(R&& r) {
			return closure{}(r);
		}
	};

	/// Collects a range into an std::vector
	inline collect_adapter collect;

	template<std::ranges::range R>
	constexpr auto operator|(R&& r, vcat::collect_adapter::closure const& a)
	{
		return a(std::forward<R>(r));
	}
}
