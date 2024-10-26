#pragma once

#include <cstddef>
#include <iterator>
#include <optional>
#include <string>

namespace dvel {
	template <typename T> using OptionalRef = std::optional<std::reference_wrapper<T>>;
	std::string indent(std::string&& input, size_t level = 1);

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
