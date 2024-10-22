#include "src/error.hh"

using Hint = dvel::Diagnostic::Hint;

namespace dvel::parser::error {
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
}
