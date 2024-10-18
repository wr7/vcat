#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <sstream>
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
					constexpr Hint(std::string msg, Span span): msg(msg), span(span) {};
					Hint() = delete;

					std::string msg;
					Span span;
			};

			constexpr Diagnostic(std::string msg, std::vector<Hint> hints): m_msg(msg), m_hints(hints) {};

			constexpr std::string_view msg() const {
				return std::string_view(this->m_msg);
			}

			constexpr std::span<const Hint> hints() const {
				return m_hints;
			}

		private:
			std::string m_msg;
			std::vector<Hint> m_hints;
	};

	class DiagnosticRenderer {
		public:
			DiagnosticRenderer(const SourceInfo& source_info, const Diagnostic& diagnostic);
			DiagnosticRenderer() = delete;

			void render_source_line(size_t line_no);
			constexpr std::stringstream& stream() {return m_stream;};
		private:
			std::stringstream m_stream;
			const SourceInfo& m_source_info;
			const Diagnostic& m_diagnostic;
			size_t            m_line_no_padding;
	};
}
