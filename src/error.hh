#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace vcat {
	namespace ansi_escape {
		constexpr std::string_view BOLD    = "\x1b[1m";
		constexpr std::string_view UNBOLD  = "\x1b[22m";
		constexpr std::string_view RED_FG  = "\x1b[31m";
		constexpr std::string_view CYAN_FG = "\x1b[36m";
		constexpr std::string_view RESET   = "\x1b[m";
	}

	// Position in source code (primarily used for error messages).
	class Span {
		public:
			constexpr inline Span(size_t start, size_t length): start(start), length(length) {};
			constexpr Span span_after() const {return Span(start + length, 0);}

			size_t start;
			size_t length;

			Span() = delete;
	};

	typedef struct {
		size_t line;
		size_t column;
	} LineAndColumn;

	// Contains precomputed line and column information and a reference to the underlying source code.
	class SourceInfo {
		public:
			SourceInfo(std::string_view src);

			constexpr std::string_view src() const {return m_src;}

			// Gets the first byte index in a line
			std::size_t start_of_line(size_t line_no) const;

			LineAndColumn position_of(size_t byte_idx) const;
			std::array<LineAndColumn, 2> position_of(Span byte_span) const;
		private:
			// The byte indexes of where each line starts
			std::vector<size_t> m_line_start_indexes;
			std::string_view    m_src;
	};

	// An object with a `Span`.
	template <typename T>
	class Spanned {
		public:
			constexpr inline Spanned<T>(T val, Span span): val(std::forward<T>(val)), span(span) {};
			T val;
			Span span;

			// C++'s lambda syntax is so bad that this probably isn't worth using like 99% of the time.
			template<typename F> // 😭
			inline Spanned<typename std::result_of<F(const T&)>::type> map(F f) const {
				return Spanned<typename std::result_of<F(const T&)>::type>(f(val), span);
			}

			// Converts a `const Spanned<T>&` into a `Spanned<const T&>`
			constexpr Spanned<const T&> as_cref() const {
				return Spanned<const T&>(val, span);
			}

			constexpr const std::remove_cvref_t<T> *operator->() const {
				return &val;
			}

			constexpr std::remove_reference_t<T> *operator->() {
				return &val;
			}

			constexpr std::add_lvalue_reference_t<T> operator*() {
				return val;
			}

			constexpr std::add_lvalue_reference_t<std::add_const_t<T>> operator*() const {
				return val;
			}
	};

	template<typename T>
	constexpr std::optional<Span> span_of(std::span<Spanned<T>> s) {
		if(s.empty()) {
			return std::optional<Span>();
		}

		const size_t start = s.front().span.start;
		const Span   end   = s.back().span;

		return Span(start, end.start + end.length - start);
	}

	template<typename T>
	constexpr std::optional<Span> span_of(std::span<const Spanned<T>> s) {
		if(s.empty()) {
			return std::optional<Span>();
		}

		const size_t start = s.front().span.start;
		const Span   end   = s.back().span;

		return Span(start, end.start + end.length - start);
	}

	// A message with one or more `Hint`s
	class Diagnostic {
		public:
			std::string render(const SourceInfo& si) const;
			// A message that points to a snippet of code
			class Hint {
				public:
					Hint() = delete;
					constexpr static Hint error(std::string&& msg, Span span) {
						return Hint(
							std::move(msg),
							span,
							ansi_escape::RED_FG,
							'^'
						);
					}

					constexpr static Hint info(std::string&& msg, Span span) {
						return Hint(
							std::move(msg),
							span,
							ansi_escape::CYAN_FG,
							'-'
						);
					}

					std::string msg;
					Span span;

					// The ANSI escape sequence for the color that the hint should be rendered with
					std::string_view m_color;

					// The character that is rendered under the offending code
					char             m_pointer_char;
				private:
					constexpr Hint(std::string msg, Span span, std::string_view color, char pointer)
						: msg(msg), span(span), m_color(color), m_pointer_char(pointer) {};
			};

			constexpr Diagnostic(std::string&& msg, std::vector<Hint> hints): m_msg(std::move(msg)), m_hints(std::move(hints)) {};

			constexpr std::string_view msg() const {
				return std::string_view(this->m_msg);
			}

			constexpr std::span<const Hint> hints() const {
				return m_hints;
			}

		private:
			std::string       m_msg;
			std::vector<Hint> m_hints;
	};
}
