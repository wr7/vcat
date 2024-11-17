#pragma once

#include "src/error.hh"
#include "src/lexer/token.hh"
#include "src/parser/expression.hh"

#include <span>

namespace dvel::parser {
	using TokenStream = std::span<const Spanned<Token>>;

	void verify_brackets(std::span<const Spanned<Token>> tokens);

	std::optional<Spanned<Expression>> try_parse_expression(TokenStream tokens);

	std::optional<Expression> try_parse_variable(TokenStream tokens);
	std::optional<Expression> try_parse_string(TokenStream tokens);
	std::optional<Expression> try_parse_parenthized_expression(TokenStream tokens);
	std::optional<Expression> try_parse_list(TokenStream tokens);
	std::optional<Expression> try_parse_function_call(TokenStream tokens);
	std::optional<Expression> try_parse_field_access(TokenStream tokens);
}
