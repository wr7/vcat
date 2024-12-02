#include "src/parser/util.hh"
#include "src/error.hh"
#include "src/lexer/token.hh"
#include <cstddef>

namespace vcat::parser {
	NonBracketedIter& NonBracketedIter::operator++() {
		if(m_nonexistant) {
			m_nonexistant = false;
			m_ptr++;
			return *this;
		}

		ptrdiff_t bracket_level = 0;
		bool      start = true;

		do {
			const Token& t = m_ptr->val;

			if(t.as_opening()) {
				bracket_level++;
			} else if(t.as_closing()) {
				bracket_level--;
			}

			if(start || bracket_level > 0) {
				m_ptr++;
				start = false;
			}
		} while(bracket_level > 0);

		return *this;
	}

	NonBracketedIter& NonBracketedIter::operator--() {
		if(m_nonexistant) {
			m_nonexistant = false;
			m_ptr--;
			return *this;
		}

		ptrdiff_t bracket_level = 0;
		bool      start = true;

		do {
			const Token& t = m_ptr->val;

			if(t.as_opening()) {
				bracket_level--;
			} else if(t.as_closing()) {
				bracket_level++;
			}

			if(start || bracket_level > 0) {
				m_ptr--;
				start = false;
			}
		} while(bracket_level > 0);

		return *this;
	}
}
