#include "src/error.hh"
#include "src/lexer/token.hh"
#include "src/parser/error.hh"
#include "src/parser.hh"

#include <cstdlib>
#include <span>

namespace dvel::parser {
	void verify_brackets(std::span<const Spanned<Token>> tokens) {
		std::vector<Spanned<BracketType>> opening_brackets;

		for(const Spanned<Token>& t : tokens) {
			if(auto bracket_type = t.val.as_opening()) {
				opening_brackets.push_back(Spanned(*bracket_type, t.span));
				continue;
			}

			if(auto bracket_type = t.val.as_closing()) {
				if(opening_brackets.empty()) {
					throw error::unopened_bracket(t.span);
				}

				Spanned<BracketType> opening = opening_brackets.back();
				opening_brackets.pop_back();

				if(opening.val != *bracket_type) {
					throw error::mismatched_brackets(opening.span, t.span);
				}
			}
		}

		if(!opening_brackets.empty()) {
			throw error::unclosed_bracket(opening_brackets.back().span);
		}
	}
}
