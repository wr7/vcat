#include "src/error.hh"
#include "src/eval/builtins.hh"
#include "src/eval/eobject.hh"
#include "src/eval/eval.hh"
#include "src/eval/error.hh"
#include "src/parser/expression.hh"

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <utility>

extern "C" {
	#include <libavutil/avstring.h>
}

namespace vcat::eval {
	const EObject& evaluate_expression(EObjectPool& pool, Spanned<const parser::Expression&> expr) {
		switch (expr.val.type()) {
			case parser::Expression::Type::Variable:
				return evaluate_variable     (pool, expr.map([](const auto& e)                {return *e.as_variable()           ;}));
			case parser::Expression::Type::Number:
				return evaluate_number       (pool, expr.map([](const auto& e) -> const auto& {return e.as_number()->get()       ;}));
			case parser::Expression::Type::String:
				return evaluate_string       (pool, expr.map([](const auto& e)                {return *e.as_string()             ;}));
			case parser::Expression::Type::List:
				return evaluate_list         (pool, expr.map([](const auto& e) -> const auto& {return e.as_list()->get()         ;}));
			case parser::Expression::Type::FunctionCall:
				return evaluate_function_call(pool, expr.map([](const auto& e) -> const auto& {return e.as_function_call()->get();}));
			case parser::Expression::Type::FieldAccess:
				throw; // unimplemented
		}

		throw;
	}

	const EObject& evaluate_string(EObjectPool& pool, Spanned<std::string_view> str) {
		return pool.add<EString>(std::string(str.val));
	}

	const EObject& evaluate_number(EObjectPool& pool, Spanned<const std::string&> expr) {
		for(char c : *expr) {
			if(!(c >= '0' && c <= '9')) {
				throw error::unexpected_character_in_number(c, expr.span);
			}
		}

		int64_t num;
		if(!av_sscanf(expr->c_str(), "%dl", &num)) {
			throw error::integer_too_large(*expr, expr.span);
		}

		return pool.add<EInteger>(num);
	}

	const EObject& evaluate_list(EObjectPool& pool, Spanned<const parser::List&> expr) {
		std::vector<Spanned<const EObject&>> elements_v;

		for(const Spanned<parser::Expression>& expr : expr.val.m_elements) {
			elements_v.push_back(Spanned<const EObject&>(evaluate_expression(pool, expr.as_cref()), expr.span));
		}

		return pool.add<EList>(std::move(elements_v));
	}

	const EObject& evaluate_variable(EObjectPool& pool, Spanned<std::string_view> expr) {
		if(expr.val == "vopen") {
			return pool.add<BuiltinFunction<builtins::vopen, "vopen">>();
		}
		if(expr.val == "concat") {
			return pool.add<BuiltinFunction<builtins::concat, "concat">>();
		}

		throw error::undefined_variable(expr);
	}

	const EObject& evaluate_function_call(EObjectPool& pool, Spanned<const parser::FunctionCall&> call) {
		const EObject& lhs = evaluate_expression(pool, call.val.m_function->as_cref());

		std::vector<Spanned<const EObject&>> args_v;

		for(const Spanned<parser::Expression>& arg : call.val.m_args) {
			args_v.push_back(Spanned<const EObject&>(evaluate_expression(pool, arg.as_cref()), arg.span));
		}

		EList args = EList(std::move(args_v));
		return lhs(pool, Spanned<const EList&>(args, call.span));
	}
}
