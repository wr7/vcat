#pragma once

#include "src/error.hh"
#include "src/eval/eobject.hh"
#include "src/parser/expression.hh"

#include <string_view>

namespace vcat::eval {
	const EObject& evaluate_expression(EObjectPool& pool, Spanned<const parser::Expression&> expr);

	const EObject& evaluate_string(EObjectPool&, Spanned<std::string_view>);
	const EObject& evaluate_list(EObjectPool&, Spanned<const parser::List&>);
	const EObject& evaluate_variable(EObjectPool&, Spanned<std::string_view>);
	const EObject& evaluate_number(EObjectPool&, Spanned<const std::string&>);
	const EObject& evaluate_function_call(EObjectPool&, Spanned<const parser::FunctionCall&>);
}
