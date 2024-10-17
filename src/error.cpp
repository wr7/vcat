#include "error.hh"
#include <cassert>
#include <iostream>
#include <string_view>

namespace dvel {
	LineInfo::LineInfo(std::string_view s) {
		m_line_start_indexes.push_back(0);

		for(size_t i = 0; i < s.length(); i++) {
			if(s[i] == '\n') {
				m_line_start_indexes.push_back(i + 1);
			}
		}
	}

	LineAndColumn LineInfo::position_of(size_t p) {
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

	std::array<LineAndColumn, 2> LineInfo::position_of(Span s) {
		const LineAndColumn start = position_of(s.start);
		const LineAndColumn end =   s.length ?
			start :
			position_of(s.start + s.length - 1);

		return {start, end};
	}
}
