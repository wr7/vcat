#pragma once

#include "sha256.h"
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace dvel {
	template <typename T> using OptionalRef = std::optional<std::reference_wrapper<T>>;
	std::string indent(std::string&& input, size_t level = 1);

	// Escapes the `"` and `\` characters in a string
	class PartiallyEscaped {
		public:
			PartiallyEscaped() = delete;
			constexpr PartiallyEscaped(std::string_view s): m_inner(s) {}

			friend std::ostream& operator<<(std::ostream& s, const dvel::PartiallyEscaped& p);
		private:
			std::string_view m_inner;
	};

	class Hasher {
		public:
			constexpr size_t pos() const {return m_nbytes;}

			void add(const void*data, size_t nbytes);
			void add(std::string_view string);
			void add(uint64_t data);
		private:
			SHA256 m_hasher;
			std::size_t m_nbytes = 0;
	};

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
