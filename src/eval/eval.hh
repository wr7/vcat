#pragma once

#include "src/error.hh"
#include "src/eval/eobject.hh"
#include "src/parser/expression.hh"

#include <string_view>

namespace vcat::eval {
	EObject& evaluate_expression(EObjectPool& pool, Spanned<const parser::Expression&> expr);

	EObject& evaluate_string(EObjectPool&, Spanned<std::string_view>);
	EObject& evaluate_list(EObjectPool&, Spanned<const parser::List&>);
	EObject& evaluate_variable(EObjectPool&, Spanned<std::string_view>);
	EObject& evaluate_function_call(EObjectPool&, Spanned<const parser::FunctionCall&>);
}
