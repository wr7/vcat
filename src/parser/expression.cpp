#include "src/parser/expression.hh"
#include "src/error.hh"
#include "src/util.hh"

#include <cstdlib>
#include <sstream>
#include <string_view>
#include <format>

namespace vcat::parser {
	Expression Expression::variable(std::string &&name) {
		Expression e;

		e.m_type = Type::Variable;
		new(&e.m_variable) std::string(std::move(name));

		return e;
	}

	Expression Expression::number(std::string &&name) {
		Expression e;

		e.m_type = Type::Number;
		new(&e.m_number) std::string(std::move(name));

		return e;
	}

	Expression Expression::string(std::string &&name) {
		Expression e;

		e.m_type = Type::String;
		new(&e.m_string) std::string(std::move(name));

		return e;
	}

	Expression Expression::list(std::vector<Spanned<Expression>>&& vals) {
		Expression e;

		e.m_type = Type::List;
		new(&e.m_list) List(std::move(vals));

		return e;
	}

	Expression Expression::function_call(Spanned<Expression>&& function, std::vector<Spanned<Expression>>&& args) {
		Expression e;

		e.m_type = Type::FunctionCall;
		new(&e.m_function_call) FunctionCall(std::move(function), std::move(args));

		return e;
	}

	Expression Expression::field_access(Spanned<Expression>&& lhs, Spanned<std::string>&& rhs) {
		Expression e;

		e.m_type = Type::FieldAccess;
		new(&e.m_field_access) FieldAccess(std::move(lhs), std::move(rhs));

		return e;
	}

	Expression Expression::set(std::vector<Set::Entry>&& entries) {
		Expression e;

		e.m_type = Type::Set;
		new(&e.m_set) Set(std::move(entries));

		return e;
	}

	Expression Expression::let(std::vector<std::tuple<std::string, Spanned<Expression>>>&& variables, std::unique_ptr<Spanned<Expression>>&& expr) {
		Expression e;

		e.m_type = Type::Let;
		new(&e.m_let) Let(std::move(variables), std::move(expr));

		return e;
	}

	std::optional<std::string_view> Expression::as_variable() const {
		if(m_type != Type::Variable) {
			return std::optional<std::string_view>();
		}

		return m_variable;
	}

	std::optional<std::reference_wrapper<const std::string>> Expression::as_number() const {
		if(m_type != Type::Number) {
			return {};
		}

		return m_number;
	}

	std::optional<std::string_view> Expression::as_string() const {
		if(m_type != Type::String) {
			return std::optional<std::string_view>();
		}

		return m_string;
	}

	OptionalRef<const List> Expression::as_list() const {
		if(m_type != Type::List) {
			return OptionalRef<const List>();
		}

		return std::cref(m_list);
	}

	OptionalRef<const FunctionCall> Expression::as_function_call() const {
		if(m_type != Type::FunctionCall) {
			return OptionalRef<const FunctionCall>();
		}

		return std::cref(m_function_call);
	}

	OptionalRef<const Set> Expression::as_set() const {
		if(m_type != Type::Set) {
			return {};
		}

		return std::cref(m_set);
	}

	OptionalRef<const Let> Expression::as_let() const {
		if(m_type != Type::Let) {
			return {};
		}

		return std::cref(m_let);
	}

	Expression::~Expression() {
		switch(m_type) {
			case Type::Variable:
				m_variable.std::string::~string(); return;
			case Type::Number:
				m_number.std::string::~string();   return;
			case Type::String:
				m_string.std::string::~string();   return;
			case Type::List:
				m_list.~List();                    return;
			case Type::FunctionCall:
				m_function_call.~FunctionCall();   return;
			case Type::FieldAccess:
				m_field_access.~FieldAccess();     return;
			case Type::Set:
				m_set.~Set();                      return;
			case Type::Let:
				m_let.~Let();                      return;
		}

		std::abort(); // unreachable
	}

	Expression::Expression(Expression&& old) {
		m_type = old.m_type;

		switch(old.m_type) {
			case Type::Variable:
				new(&m_variable) std::string(std::move(old.m_variable));
				return;
			case Type::Number:
				new(&m_number) std::string(std::move(old.m_number));
				return;
			case Type::String:
				new(&m_string) std::string(std::move(old.m_string));
				return;
			case Type::List:
				new(&m_list) List(std::move(old.m_list));
				return;
			case Type::FunctionCall:
				new(&m_function_call) FunctionCall(std::move(old.m_function_call));
				return;
			case Type::FieldAccess:
				new(&m_field_access) FieldAccess(std::move(old.m_field_access));
				return;
			case Type::Set:
				new(&m_set) Set(std::move(old.m_set));
				return;
			case Type::Let:
				new(&m_let) Let(std::move(old.m_let));
				return;
		}

		std::abort(); // unreachable
	}

	std::string List::to_string() const {
		if(m_elements.empty()) {
			return "List []\n";
		}

		std::stringstream s;

		s << "List [\n";
		for(const Spanned<Expression>& e: m_elements) {
			s << indent(e.val.to_string()) << ",\n";
		}
		s << "]";

		return std::move(s).str();
	}

	std::string FunctionCall::to_string() const {
		std::stringstream s;

		s
			<< "Call {\n"
			<< "  function: (\n"
			<< indent(m_function->val.to_string(), 2)
			<< "\n  ),\n"
			<< "  args: [";

		if(m_args.empty()) {
			s << "]\n}";
			return std::move(s).str();
		} else {
			s << "\n";
		}

		for(const Spanned<Expression>& a: m_args) {
			s << indent(a.val.to_string(), 2) << ",\n";
		}

		s
			<< "  ]\n"
			<< "}";

		return std::move(s).str();
	}

	std::string FieldAccess::to_string() const {
		std::stringstream s;

		s
			<< "FieldAccess {\n"
			<< "  lhs: (\n"
			<< indent(m_lhs->val.to_string(), 2)
			<< "\n  ),\n"
			<< "  rhs: \"" << m_rhs.val << "\"\n"
			<< "}";

		return std::move(s).str();
	}

	std::string Set::to_string() const {
		if(m_entries.empty()) {
			return "Set {}\n";
		}

		std::stringstream s;

		s << "Set {\n";
		for(const Entry& e: m_entries) {
			s << indent(*e.key + ": " + e.value->to_string()) << ",\n";
		}
		s << "}";

		return std::move(s).str();
	}

	std::string Let::to_string() const {
		std::stringstream s;

		s << "Let {\n";
		for(const auto& e: m_variables) {
			s << indent(std::get<0>(e)+ " = " + std::get<1>(e)->to_string()) << ";\n";
		}

		if(!m_variables.empty()) {
			s << '\n';
		}

		s << indent("in: " + m_expr->val.to_string()) << '\n';

		s << "}";

		return std::move(s).str();
	}

	std::string Expression::to_string() const {
		switch(m_type) {
			case Type::Variable:
				return std::format("Variable({})", m_variable);
			case Type::Number:
				return std::format("Number({})", m_number);
			case Type::String:
				return std::format("String({})", m_variable);
			case Type::List:
				return m_list.to_string();
			case Type::FunctionCall:
				return m_function_call.to_string();
			case Type::FieldAccess:
				return m_field_access.to_string();
			case Type::Set:
				return m_set.to_string();
			case Type::Let:
				return m_let.to_string();
		}

		std::abort(); // unreachable
	}
}
