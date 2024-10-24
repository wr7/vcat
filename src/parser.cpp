#include "src/error.hh"
#include "src/lexer/token.hh"
#include "src/parser/error.hh"
#include "src/parser/expression.hh"
#include "src/parser/util.hh"
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

	std::optional<Spanned<Expression>> try_parse_expression(TokenStream tokens) {
		if(tokens.empty()) {
			return std::optional<Spanned<Expression>>();
		}

		typedef std::optional<Expression> (*subfunction)(TokenStream);
		constexpr subfunction subfunctions[] = {
			try_parse_variable,
			try_parse_set,
			// try_parse_function_call, // unimplemented
		};

		std::optional<Expression> expression;
		for(subfunction f: subfunctions) {
			if(std::optional<Expression> e = f(tokens)) {
				expression.emplace(std::move(*e));
				break;
			}
		}

		Span span = *span_of(tokens);

		if(!expression.has_value()) {
			throw error::invalid_expression(span);
		}

		return Spanned(std::move(*expression), span);
	}

	std::optional<Expression> try_parse_variable(TokenStream tokens) {
		if(tokens.size() != 1) {
			return std::optional<Expression>();
		}

		if(std::optional<std::string_view> ident = tokens.front().val.as_identifier()) {
			return Expression::variable(std::string(*ident));
		}

		return std::optional<Expression>();
	}

	std::optional<Expression> try_parse_set(TokenStream tokens) {
		if(tokens.empty()) {
			return std::optional<Expression>();
		}

		if(
			   tokens.front().val != Token("[")
			|| tokens.back() .val != Token("]")
		) {
			return std::optional<Expression>();
			
		}

		const TokenStream inside = tokens.subspan(1, tokens.size() - 2);
		size_t element_start = 0;

		std::vector<Spanned<Expression>> elements;

		for(const Spanned<Token> *tok_ptr : NonBracketed(inside)) {
			const Spanned<Token>& tok = *tok_ptr;
			const size_t i = tok_ptr - inside.data();

			if(tok.val == Token(",")) {
				const size_t len = i - element_start;
				const TokenStream element = inside.subspan(element_start, len);

				std::optional<Spanned<Expression>> element_expr = try_parse_expression(element);
				const Span s = span_of(element).value_or(tok.span);

				if(!element_expr.has_value()) {
					throw parser::error::expected_expression(s);
				}

				elements.push_back(std::move(*element_expr));
				element_start = i + 1;
			}
		}

		const size_t len = inside.size() - element_start;
		const TokenStream element = inside.subspan(element_start, len);

		if(std::optional<Spanned<Expression>> element_expr = try_parse_expression(element)) {
			elements.push_back(std::move(*element_expr));
		}

		return Expression::set(std::move(elements));
	}

	std::optional<Expression> try_parse_function_call(TokenStream tokens) {
		throw; // unimplemented
	}
}
