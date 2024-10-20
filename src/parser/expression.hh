#pragma once

#include "src/util.hh"

#include <memory>
#include <string_view>
#include <vector>

namespace dvel::parser {
	class Expression;

	struct Set {
		public:
			Set(Set&) = delete;
			Set(Set&&) = default;

			std::string to_string() const;
			std::vector<Expression> m_elements;
	};

	struct FunctionCall {
		FunctionCall(FunctionCall&) = delete;
		FunctionCall(FunctionCall&&) = default;

		std::string to_string() const;
		inline FunctionCall(Expression&& function, std::vector<Expression>&& args);

		std::unique_ptr<Expression> m_function;
		std::vector<Expression>     m_args;
	};

	class Expression {
		public:
			static Expression variable(std::string&&);
			static Expression set(std::vector<Expression>&&);
			static Expression function_call(Expression&& function, std::vector<Expression>&& args);

			std::string to_string() const;

			std::optional<std::string_view> as_variable() const;
			OptionalRef<const Set> as_set() const;
			OptionalRef<const FunctionCall> as_function_call() const;

			Expression(Expression&&);
			~Expression();
		private:
			constexpr Expression() {}

			enum struct Type {
				Variable,
				Set,
				FunctionCall,
			};

			Type m_type;

			union {
				std::string  m_variable;
				Set          m_set;
				FunctionCall m_function_call;
			};
	};

	inline FunctionCall::FunctionCall(Expression&& function, std::vector<Expression>&& args)
		: m_function(new Expression(std::move(function)))
		, m_args(std::move(args)) {}
}
