#pragma once

#include <format>
#include "src/error.hh"
#include "src/lexer/token.hh"

using Hint = vcat::Diagnostic::Hint;

namespace vcat::parser::error {
	constexpr Diagnostic unclosed_bracket(Span span) {
		return Diagnostic(
			"Bracket is not closed",
			{
				Hint::error("Opening bracket here", span)
			}
		);
	}

	constexpr Diagnostic unopened_bracket(Span span) {
		return Diagnostic(
			"Closing bracket does not have a corresponding open bracket",
			{
				Hint::error("Closing bracket here", span)
			}
		);
	}

	constexpr Diagnostic mismatched_brackets(Span opening, Span closing) {
		return Diagnostic(
			"Error: matching brackets are of different types",
			{
				Hint::error("Opening bracket here", opening),
				Hint::error("Closing bracket here", closing)
			}
		);
	}

	constexpr Diagnostic invalid_expression(Span span) {
		return Diagnostic(
			"Invalid expression",
			{
				Hint::error("", span),
			}
		);
	}

	constexpr Diagnostic expected_expression(Span span) {
		return Diagnostic(
			"Expected expression",
			{
				Hint::error("Expected expression here", span),
			}
		);
	}

	inline Diagnostic expected_attribute_name(Spanned<const Token&> got) {
		return Diagnostic(
			std::format("Expected attribute name (string or identifier); got `{}`", got->to_string()),
			{
				Hint::error("", got.span),
			}
		);
	}

	inline Diagnostic expected_variable_name(Spanned<const Token&> got) {
		return Diagnostic(
			std::format("Expected variable name (identifier); got `{}`", got->to_string()),
			{
				Hint::error("", got.span),
			}
		);
	}

	inline Diagnostic expected_token(const Token& expected, Spanned<const Token&> got) {
		return Diagnostic(
			std::format("Expected token `{}`; got `{}`", expected.to_string(), got->to_string()),
			{
				Hint::error("", got.span)
			}
		);
	}

	inline Diagnostic expected_token(const Token& expected, Span got) {
		return Diagnostic(
			std::format("Expected token `{}`", expected.to_string()),
			{
				Hint::error("", got)
			}
		);
	}

	inline Diagnostic expected_in(Span s) {
		return Diagnostic(
			"Expected `in` for `let` expression",
			{
				Hint::error("`let` here", s)
			}
		);
	}

	inline Diagnostic expected_variable_declaration(Span s) {
		return Diagnostic(
			"Expected variable declaration; got `;`",
			{
				Hint::error("", s)
			}
		);
	}
}
