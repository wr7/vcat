#include "src/error.hh"
#include "src/eval/builtins.hh"
#include "src/eval/eobject.hh"
#include "src/eval/eval.hh"
#include "src/eval/error.hh"
#include "src/parser/expression.hh"

#include <memory>
#include <utility>

namespace vcat::eval {
	std::unique_ptr<EObject> evaluate_expression(Spanned<const parser::Expression&> expr) {
		switch (expr.val.type()) {
			case parser::Expression::Type::Variable:
				return evaluate_variable     (expr.map([](const auto& e)                {return *e.as_variable()           ;}));
			case parser::Expression::Type::String:
				return evaluate_string       (expr.map([](const auto& e)                {return *e.as_string()             ;}));
			case parser::Expression::Type::List:
				return evaluate_list         (expr.map([](const auto& e) -> const auto& {return e.as_list()->get()         ;}));
			case parser::Expression::Type::FunctionCall:
				return evaluate_function_call(expr.map([](const auto& e) -> const auto& {return e.as_function_call()->get();}));
			case parser::Expression::Type::FieldAccess:
				throw; // unimplemented
		}

		throw;
	}

	std::unique_ptr<EObject> evaluate_string(Spanned<std::string_view> str) {
		return std::make_unique<EString>(std::string(str.val));
	}

	std::unique_ptr<EObject> evaluate_list(Spanned<const parser::List&> expr) {
		std::vector<Spanned<std::unique_ptr<EObject>>> elements_v;

		for(const Spanned<parser::Expression>& expr : expr.val.m_elements) {
			elements_v.push_back(Spanned(evaluate_expression(expr.as_cref()), expr.span));
		}

		return std::make_unique<EList>(std::move(elements_v));
	}

	std::unique_ptr<EObject> evaluate_variable(Spanned<std::string_view> expr) {
		if(expr.val == "vopen") {
			return std::make_unique<BuiltinFunction<builtins::vopen, "vopen">>();
		}
		if(expr.val == "concat") {
			return std::make_unique<BuiltinFunction<builtins::concat, "concat">>();
		}

		throw error::undefined_variable(expr);
	}

	std::unique_ptr<EObject> evaluate_function_call(Spanned<const parser::FunctionCall&> call) {
		std::unique_ptr<EObject> lhs = evaluate_expression(call.val.m_function->as_cref());

		std::vector<Spanned<std::unique_ptr<EObject>>> args_v;

		for(const Spanned<parser::Expression>& arg : call.val.m_args) {
			args_v.push_back(Spanned(evaluate_expression(arg.as_cref()), arg.span));
		}

		EList args = EList(std::move(args_v));
		return (*lhs.get())(Spanned<EList&>(args, call.span));
	}
}
