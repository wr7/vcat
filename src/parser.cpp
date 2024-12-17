#include "src/error.hh"
#include "src/lexer/token.hh"
#include "src/parser/error.hh"
#include "src/parser/expression.hh"
#include "src/parser/util.hh"
#include "src/parser.hh"

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <span>
#include <string>

namespace vcat::parser {
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
			try_parse_number,
			try_parse_string,
			try_parse_parenthized_expression,
			try_parse_function_call,
			try_parse_set,
			try_parse_list,
			try_parse_field_access,
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

	std::optional<Expression> try_parse_number(TokenStream tokens) {
		if(tokens.size() != 1) {
			return std::optional<Expression>();
		}

		if(std::optional<std::string_view> num = tokens.front().val.as_number()) {
			return Expression::number(std::string(*num));
		}

		return std::optional<Expression>();
	}

	std::optional<Expression> try_parse_string(TokenStream tokens) {
		if(tokens.size() != 1) {
			return std::optional<Expression>();
		}

		if(std::optional<std::string_view> ident = tokens.front().val.as_string()) {
			return Expression::string(std::string(*ident));
		}

		return std::optional<Expression>();
	}

	std::optional<Expression> try_parse_parenthized_expression(TokenStream tokens) {
		auto iter = NonBracketed(tokens);

		auto opening_paren = iter.next();
		if(!opening_paren || *opening_paren->get() != Token("(")) {
			return {};
		}

		auto closing_paren = iter.next();
		assert(closing_paren && *closing_paren->get() == Token(")"));

		if(iter.next()) {
			return {};
		}

		TokenStream inside = tokens.subspan(1, tokens.size() - 2);
		std::optional<Spanned<Expression>> expr = try_parse_expression(inside);

		if(!expr) {
			return std::optional<Expression>();
		}

		return std::move(expr->val);
	}

	static std::vector<Set::Entry> parse_set_entries(TokenStream tokens);

	std::optional<Expression> try_parse_set(TokenStream tokens) {
		NonBracketed iter = tokens;

		auto opening_brace = iter.next();
		if(!opening_brace || *opening_brace->get() != Token("{")) {
			return {};
		}

		auto closing_brace = iter.next();
		assert(closing_brace && *closing_brace->get() == Token("}"));

		if(iter.next()) {
			return {};
		}

		TokenStream entry_tokens = tokens.subspan(1, tokens.size() - 2);
		std::vector<Set::Entry> entries = parse_set_entries(entry_tokens);

		return Expression::set(std::move(entries));
	}

	static std::vector<Spanned<Expression>> parse_expression_list(TokenStream tokens);

	std::optional<Expression> try_parse_list(TokenStream tokens) {
		auto iter = NonBracketed(tokens);

		auto opening_bracket = iter.next();
		if(!opening_bracket || *opening_bracket->get() != Token("[")) {
			return {};
		}

		auto closing_bracket = iter.next();
		assert(closing_bracket && *closing_bracket->get() == Token("]"));

		if(iter.next()) {
			return {};
		}

		const TokenStream inside = tokens.subspan(1, tokens.size() - 2);

		std::vector<Spanned<Expression>> elements = parse_expression_list(inside);

		return Expression::list(std::move(elements));
	}

	std::optional<Expression> try_parse_function_call(TokenStream tokens) {
		auto iter = Reversed(NonBracketed(tokens));

		auto closing_paren = iter.next();
		if(!closing_paren || *closing_paren->get() != Token(")")) {
			return {};
		}
		const size_t closing_idx = &closing_paren->get() - tokens.data();

		auto opening_paren = iter.next();
		assert(opening_paren && *opening_paren->get() == Token("("));
		const size_t opening_idx = &opening_paren->get() - tokens.data();

		TokenStream function_tokens = tokens.subspan(0, opening_idx);
		std::optional<Spanned<Expression>> function = try_parse_expression(function_tokens);

		if(!function.has_value()) {
			return std::optional<Expression>();
		}

		TokenStream params_tokens = tokens.subspan(opening_idx + 1, closing_idx - opening_idx - 1);
		std::vector<Spanned<Expression>> params = parse_expression_list(params_tokens);

		return Expression::function_call(std::move(*function), std::move(params));
	}

	std::optional<Expression> try_parse_field_access(TokenStream tokens) {
		if(tokens.size() < 3) {
			return std::optional<Expression>();
		}

		const std::optional<std::string_view> rhs = tokens.back().val.as_identifier();
		if(!rhs) {
			return std::optional<Expression>();
		}
		const Span rhs_span = tokens.back().span;

		if(tokens[tokens.size() - 2].val != Token(".")) {
			return std::optional<Expression>();
		}

		const TokenStream lhs_tok = tokens.subspan(0, tokens.size() - 2);
		std::optional<Spanned<Expression>> lhs = try_parse_expression(lhs_tok);
		assert(lhs);

		return Expression::field_access(std::move(*lhs), Spanned(std::string(*rhs), rhs_span));
	}

	static std::vector<Spanned<Expression>> parse_expression_list(TokenStream tokens) {
		std::vector<Spanned<Expression>> expressions;

		size_t expr_start = 0;

		for(NonBracketed iter = tokens; auto tok_ref = iter.next();) {
			const Spanned<Token>& tok = *tok_ref;
			const size_t i = &tok_ref->get() - tokens.data();

			if(tok.val == Token(",")) {
				const size_t len = i - expr_start;
				const TokenStream expr_tokens = tokens.subspan(expr_start, len);

				std::optional<Spanned<Expression>> expr = try_parse_expression(expr_tokens);
				const Span s = span_of(expr_tokens).value_or(tok.span);

				if(!expr.has_value()) {
					throw parser::error::expected_expression(s);
				}

				expressions.push_back(std::move(*expr));
				expr_start = i + 1;
			}
		}

		const size_t len = tokens.size() - expr_start;
		const TokenStream expr_tokens = tokens.subspan(expr_start, len);

		if(std::optional<Spanned<Expression>> expr = try_parse_expression(expr_tokens)) {
			expressions.push_back(std::move(*expr));
		}

		return expressions;
	}

	static std::optional<Set::Entry> parse_set_entry(TokenStream tokens) {
		auto iter = NonBracketed(tokens);

		auto name_opt = iter.next();
		if(!name_opt) {
			return {};
		}

		const Spanned<Token>& name_tok = *name_opt;
		std::string name;

		if(name_tok->as_identifier()) {
			name = *name_tok->as_identifier();
		} else if(name_tok->as_string()) {
			name = *name_tok->as_string();
		} else {
			throw error::expected_attribute_name(name_tok.as_cref());
		}

		auto equal_opt = iter.next();

		if(!equal_opt) {
			throw error::expected_token(Token("="), name_tok.span.span_after());
		}

		if(*equal_opt->get() != Token("=")) {
			throw error::expected_token(Token("="), equal_opt->get().as_cref());
		}

		TokenStream expr_tokens = tokens.subspan(2);
		std::optional<Spanned<Expression>> expr = try_parse_expression(expr_tokens);

		if(!expr) {
			throw error::expected_expression(equal_opt->get().span.span_after());
		}

		return Set::Entry(
			Spanned(std::move(name), name_tok.span),
			std::move(*expr)
		);
	}

	static std::vector<Set::Entry> parse_set_entries(TokenStream tokens) {
		std::vector<Set::Entry> entries;

		size_t entry_start = 0;

		NonBracketed iter = tokens;
		OptionalRef<const Spanned<Token>> tok;

		do {
			tok = iter.next();
			const size_t i = tok
				? &tok->get() - tokens.data()
				: tokens.size();

			if(!tok || *tok->get() == Token(",") || *tok->get() == Token(";")) {
				size_t len = i - entry_start;
				const TokenStream expr_tokens = tokens.subspan(entry_start, len);

				std::optional<Set::Entry> entry = parse_set_entry(expr_tokens);

				if(!tok && !entry) {
					break; // Ignore trailing comma
				} else if(tok && !entry) {
					throw parser::error::expected_attribute_name(tok->get().as_cref());
				}

				entries.push_back(std::move(*entry));
				entry_start = i + 1;
			}

		} while(tok);

		return entries;
	}
}
