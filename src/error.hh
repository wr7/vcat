#pragma once

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
