#include "error.hh"
#include <cassert>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <algorithm>

namespace dvel {
	// Helper class for rendering diagnostics; functionality is publicly accessable via the
	// `Diagnostic::render` method.
	class DiagnosticRenderer {
		public:
			DiagnosticRenderer(const SourceInfo& source_info, const Diagnostic& diagnostic);
			DiagnosticRenderer() = delete;

			constexpr std::stringstream& stream() {return m_stream;};
			void render();
		private:
			std::stringstream m_stream;
			const SourceInfo& m_source_info;
			const Diagnostic& m_diagnostic;
			size_t            m_line_no_padding;

			// Renders a line of source code; returns the length of the line.
			size_t render_source_line(size_t line_no);

			// Renders an empty gutter and sets the text to bold.
			void render_empty_gutter();

			// Renders a hint and its accompanying source code
			void render_hint(const Diagnostic::Hint& hint);
	};

	std::string Diagnostic::render(const SourceInfo& source_info) const {
		DiagnosticRenderer renderer(source_info, *this);

		renderer.render();

		return std::move(renderer.stream()).str();
	}

	void DiagnosticRenderer::render() {
		m_stream
			<< ' '
			<< m_diagnostic.msg()
			<< "\n\n";

		size_t previous_line_no = 0;

		for(const Diagnostic::Hint& h : m_diagnostic.hints()) {
			const std::array<LineAndColumn, 2> line_info = m_source_info.position_of(h.span);

			size_t n_prelude_lines = line_info[0].line - std::min(line_info[0].line, previous_line_no + 1);
			n_prelude_lines        = std::min(n_prelude_lines, (size_t) 2);

			if(previous_line_no + n_prelude_lines + 1 != line_info[0].line) {
				m_stream << ansi_escape::BOLD;
				for(size_t i = 0; i < m_line_no_padding; i++) {
					m_stream << ' ';
				}
				m_stream << "..." << ansi_escape::RESET << "\n";
			}

			for(size_t i = n_prelude_lines; i > 0; i--) {
				render_source_line(line_info[0].line - i);
			}

			render_hint(h);
			previous_line_no = line_info[1].line;
		}
	}

	void DiagnosticRenderer::render_hint(const Diagnostic::Hint& hint) {
		const std::array<LineAndColumn, 2> line_info = m_source_info.position_of(hint.span);

		for(size_t line_no = line_info[0].line; line_no <= line_info[1].line; line_no++) {
			const size_t line_length = render_source_line(line_no);

			size_t col_start = line_info[0].column;
			size_t col_end   = line_info[1].column;

			if(line_no != line_info[0].line) {
				col_start = 1;
			}
			if(line_no != line_info[1].line) {
				col_end = std::max(line_length, (size_t) 1);
			}
			assert(col_end >= col_start);

			render_empty_gutter();
			m_stream << hint.m_color;

			for(size_t i = 0; i < col_start - 1; i++) {
				m_stream << ' ';
			}
			for(size_t i = 0; i < col_end - col_start + 1; i++) {
				m_stream << hint.m_pointer_char;
			}

			m_stream
				<< ansi_escape::RESET
				<< '\n';
		}

		if(hint.msg.empty()) {
			return;
		}

		render_empty_gutter();
		m_stream
			<< hint.m_color
			<< hint.msg
			<< ansi_escape::RESET
			<< "\n";
	}

	void DiagnosticRenderer::render_empty_gutter() {
		m_stream << ansi_escape::BOLD;
		for(size_t i = 0; i < m_line_no_padding; i++) {
			m_stream << ' ';
		}

		m_stream
			<< " | ";
	}

	size_t DiagnosticRenderer::render_source_line(size_t line_no) {
		m_stream
			<< ansi_escape::BOLD
			<< std::setfill(' ')
			<< std::setw(m_line_no_padding)
			<< line_no
			<< " | "
			<< ansi_escape::UNBOLD;

		const size_t line_start = m_source_info.start_of_line(line_no);
		size_t line_length = 0;

		for(char c : m_source_info.src().substr(line_start)) {
			if(c == '\n') {
				break;
			}

			m_stream << c;
			line_length++;
		}

		m_stream << '\n';

		return line_length;
	}

	DiagnosticRenderer::DiagnosticRenderer(const SourceInfo& source_info, const Diagnostic& diagnostic) : m_source_info(source_info), m_diagnostic(diagnostic) {
		const Span   last_hint_span = diagnostic.hints().back().span;
		const size_t max_line_no = m_source_info.position_of(last_hint_span)[1].line;

		// Calculate padding for line numbers
		size_t n = max_line_no;
		m_line_no_padding = 1;
		do {
			n /= 10;
			m_line_no_padding += 1;
		} while(n != 0);
	}

	SourceInfo::SourceInfo(std::string_view src): m_src(src) {
		m_line_start_indexes.push_back(0);

		for(size_t i = 0; i < src.length(); i++) {
			if(src[i] == '\n') {
				m_line_start_indexes.push_back(i + 1);
			}
		}
	}

	size_t SourceInfo::start_of_line(const size_t line_no) const {
		assert(line_no != 0);

		const size_t line_idx = line_no - 1;
		assert(line_idx < m_line_start_indexes.size());

		return m_line_start_indexes[line_idx];
	}

	LineAndColumn SourceInfo::position_of(size_t p) const {
		// Binary search for line number //
		size_t base = 0;
		size_t size = m_line_start_indexes.size();

		assert(size > 0);

		while(size > 1) {
			size_t half = size / 2;
			size_t mid =  base + half;

			if(p >= m_line_start_indexes[mid]) {
				base = mid;
			}

			size -= half;
		}

		assert(p >= m_line_start_indexes[base]);

		LineAndColumn info;

		info.line   = base + 1;
		info.column = p - m_line_start_indexes[base] + 1;

		return info;
	}

	std::array<LineAndColumn, 2> SourceInfo::position_of(Span s) const {
		const LineAndColumn start = position_of(s.start);
		const LineAndColumn end =   s.length == 0 ?
			start :
			position_of(s.start + s.length - 1);

		return {start, end};
	}
}
