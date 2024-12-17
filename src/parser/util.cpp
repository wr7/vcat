#include "src/parser/util.hh"
#include "src/error.hh"
#include "src/lexer/token.hh"
#include "src/util.hh"
#include <cstddef>
#include <iostream>

namespace vcat::parser {
	OptionalRef<const Spanned<Token>> NonBracketed::next() {
		if(m_remaining.empty()) {
			return {};
		}

		const Spanned<Token>& retval = m_remaining.front();
		m_remaining = m_remaining.subspan(1);

		if(!retval->as_opening()) {
			return retval;
		}

		for(size_t nesting_level = 0;;m_remaining = m_remaining.subspan(1)) {
			if(m_remaining.empty()) {
				std::cerr << "Internal error: NonBracketed used on TokenStream with unmatched brackets\n";
				std::abort();
			}

			const Spanned<Token>& tok = m_remaining.front();

			if(tok->as_opening()) {
				nesting_level += 1;
			} else if(tok->as_closing()) {
				if(nesting_level == 0) {
					break;
				} else {
					nesting_level -= 1;
				}
			}
		}

		return retval;
	}

	OptionalRef<const Spanned<Token>> NonBracketed::next_back() {
		if(m_remaining.empty()) {
			return {};
		}

		const Spanned<Token>& retval = m_remaining.back();
		m_remaining = m_remaining.subspan(0, m_remaining.size() - 1);

		if(!retval->as_closing()) {
			return retval;
		}

		for(size_t nesting_level = 0;;m_remaining = m_remaining.subspan(0, m_remaining.size() - 1)) {
			if(m_remaining.empty()) {
				std::cerr << "Internal error: NonBracketed used on TokenStream with unmatched brackets\n";
				std::abort();
			}

			const Spanned<Token>& tok = m_remaining.back();

			if(tok->as_closing()) {
				nesting_level += 1;
			} else if(tok->as_opening()) {
				if(nesting_level == 0) {
					break;
				} else {
					nesting_level -= 1;
				}
			}
		}

		return retval;
	}
}
