#include "error.hh"
#include <cassert>
#include <cmath>
#include <iomanip>
#include <string_view>

namespace dvel {
	void DiagnosticRenderer::render_source_line(size_t line_no) {
		m_stream
			<< " \x1b[1m"   // Bold text
			<< std::setfill(' ')
			<< std::setw(m_line_no_padding)
			<< line_no
			<< " | \x1b[m"; // Reset formatting

		const size_t line_start = m_source_info.start_of_line(line_no);

		for(size_t i = line_start; i < m_source_info.src().length() ; i++) {
			char c = m_source_info.src()[i];

			if(c == '\n') {
				break;
			}

			m_stream << c;
		}

		m_stream << '\n';
	}

	DiagnosticRenderer::DiagnosticRenderer(const SourceInfo& source_info, const Diagnostic& diagnostic) : m_source_info(source_info), m_diagnostic(diagnostic) {
		const Span   last_hint_span = diagnostic.hints().back().span;
		const size_t max_line_no = m_source_info.position_of(last_hint_span)[1].line;

		// Calculate padding for line numbers
		size_t n = max_line_no;
		m_line_no_padding = 0;
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
		const LineAndColumn end =   s.length ?
			start :
			position_of(s.start + s.length - 1);

		return {start, end};
	}
}
