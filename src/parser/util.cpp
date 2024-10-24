#include "src/parser/util.hh"
#include "src/error.hh"
#include "src/lexer/token.hh"

namespace dvel::parser {
	NonBracketedIter& NonBracketedIter::operator++() {
		if(m_nonexistant) {
			m_nonexistant = false;
			m_ptr++;

			return *this;
		}

		size_t bracket_level = 0;

		do {
			const Token& t = m_ptr->val;

			if(t.as_opening()) {
				bracket_level++;
			} else if(t.as_closing()) {
				bracket_level--;
			}

			m_ptr++;
		} while(bracket_level > 0);

		return *this;
	}

	NonBracketedIter& NonBracketedIter::operator--() {
		if(m_nonexistant) {
			m_nonexistant = false;
			m_ptr--;

			return *this;
		}

		size_t bracket_level = 0;

		do {
			const Token& t = m_ptr->val;

			if(t.as_closing()) {
				bracket_level++;
			} else if(t.as_opening()) {
				bracket_level--;
			}

			m_ptr--;
		} while(bracket_level > 0);

		return *this;
	}
}
