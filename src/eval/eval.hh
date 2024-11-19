#pragma once

#include "src/error.hh"
#include "src/eval/eobject.hh"
#include "src/parser/expression.hh"

#include <string_view>

namespace dvel::eval {
	std::unique_ptr<EObject> evaluate_expression(Spanned<const parser::Expression&> expr);

	std::unique_ptr<EObject> evaluate_string(Spanned<std::string_view>);
	std::unique_ptr<EObject> evaluate_list(Spanned<const parser::List&>);
	std::unique_ptr<EObject> evaluate_variable(Spanned<std::string_view>);
	std::unique_ptr<EObject> evaluate_function_call(Spanned<const parser::FunctionCall&>);
}
