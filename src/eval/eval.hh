#pragma once

#include "src/error.hh"
#include "src/eval/eobject.hh"
#include "src/eval/scope.hh"
#include "src/parser/expression.hh"

#include <string_view>

namespace vcat::eval {
	const EObject& evaluate_expression(EObjectPool& pool, const Scope&, Spanned<const parser::Expression&> expr);

	const EObject& evaluate_string(EObjectPool&, Spanned<std::string_view>);
	const EObject& evaluate_list(EObjectPool&, const Scope&, Spanned<const parser::List&>);
	const EObject& evaluate_variable(EObjectPool&, const Scope&, Spanned<std::string_view>);
	const EObject& evaluate_number(EObjectPool&, Spanned<const std::string&>);
	const EObject& evaluate_function_call(EObjectPool&, const Scope&, Spanned<const parser::FunctionCall&>);
	const EObject& evaluate_let(EObjectPool&, const Scope&, Spanned<const parser::Let&>);
}
