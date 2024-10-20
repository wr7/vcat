#include "src/parser/expression.hh"
#include "src/parser/error.hh"

#include <cstdlib>
#include <string_view>

namespace dvel::parser {
	Expression Expression::variable(std::string &&name) {
		Expression e;

		e.m_type = Type::Variable;
		new(&e.m_variable) std::string(std::move(name));

		return e;
	}

	Expression Expression::set(std::vector<Expression>&& vals) {
		Expression e;

		e.m_type = Type::Set;
		new(&e.m_set) std::vector<Expression>(std::move(vals));

		return e;
	}

	Expression Expression::function_call(Expression&& function, std::vector<Expression>&& args) {
		Expression e;

		e.m_type = Type::FunctionCall;
		new(&e.m_function_call) FunctionCall(std::move(function), std::move(args));

		return e;
	}

	std::optional<std::string_view> Expression::as_variable() const {
		if(m_type != Type::Variable) {
			return std::optional<std::string_view>();
		}

		return m_variable;
	}

	OptionalRef<const Set> Expression::as_set() const {
		if(m_type != Type::Set) {
			return OptionalRef<const Set>();
		}

		return std::cref(m_set);
	}

	OptionalRef<const FunctionCall> Expression::as_function_call() const {
		if(m_type != Type::FunctionCall) {
			return OptionalRef<const FunctionCall>();
		}

		return std::cref(m_function_call);
	}

	Expression::~Expression() {
		switch(m_type) {
			case Type::Variable:
				m_variable.std::string::~string(); return;
			case Type::Set:
				m_set.~Set();                      return;
			case Type::FunctionCall:
				m_function_call.~FunctionCall();   return;
		}

		std::abort(); // unreachable
	}

	Expression::Expression(Expression&& old) {
		switch(old.m_type) {
			case Type::Variable:
				new(&m_variable) std::string(std::move(old.m_variable));
				return;
			case Type::Set:
				new(&m_set) Set(std::move(old.m_set));
				return;
			case Type::FunctionCall:
				new(&m_function_call) FunctionCall(std::move(old.m_function_call));
				return;
		}

		std::abort(); // unreachable
	}
}
