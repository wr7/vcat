#pragma once

#include <string>
#include <vector>

namespace dvel {
	class Span {
		public:
			constexpr inline Span(size_t start, size_t length): start(start), length(length) {};
			size_t start;
			size_t length;
	};

	template <typename T>
	class Spanned {
		public:
			constexpr inline Spanned<T>(T val, Span span): val(val), span(span) {};
			T val;
			Span span;
	};

	class Diagnostic {
		public:
			class Hint {
				public:
					constexpr inline Hint(std::string msg, Span span): msg(msg), span(span) {};
				private:
					std::string msg;
					Span span;
			};

			constexpr inline Diagnostic(std::string msg, std::vector<Hint> hints): msg(msg), hints(hints) {};

		private:
			std::string msg;
			std::vector<Hint> hints;
	};
}
