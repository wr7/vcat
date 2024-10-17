#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace dvel {
	// Position in source code (primarily used for error messages).
	class Span {
		public:
			constexpr inline Span(size_t start, size_t length): start(start), length(length) {};
			size_t start;
			size_t length;
			Span() = delete;
	};

	typedef struct {
		size_t line;
		size_t column;
	} LineAndColumn;

	// Precomputed information for determining the line and column number of a `Span`
	class LineInfo {
		public:
			LineInfo(std::string_view);

			LineAndColumn position_of(size_t);
			std::array<LineAndColumn, 2> position_of(Span);
		private:
			// The byte indexes of where each line starts
			std::vector<size_t> m_line_start_indexes;
	};

	// An object with a `Span`.
	template <typename T>
	class Spanned {
		public:
			constexpr inline Spanned<T>(T val, Span span): val(std::move(val)), span(span) {};
			T val;
			Span span;
	};

	// A message with one or more `Hint`s
	class Diagnostic {
		public:
			// A message that points to a snippet of code
			class Hint {
				public:
					constexpr inline Hint(std::string msg, Span span): msg(msg), span(span) {};
				private:
					std::string msg;
					Span span;
			};

			constexpr inline Diagnostic(std::string msg, std::vector<Hint> hints): m_msg(msg), m_hints(hints) {};

			constexpr std::string_view msg() {
				return std::string_view(this->m_msg);
			}

		private:
			std::string m_msg;
			std::vector<Hint> m_hints;
	};
}
