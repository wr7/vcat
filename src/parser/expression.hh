#pragma once

#include "src/error.hh"
#include "src/util.hh"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace vcat::parser {
	class Expression;

	struct List {
		public:
			List(List&) = delete;
			List(List&&) = default;

			inline List(std::vector<Spanned<Expression>>&& entries);

			std::string to_string() const;
			std::vector<Spanned<Expression>> m_elements;
	};

	struct Set {
		struct Entry;

		public:
			Set(Set&) = delete;
			Set(Set&&) = default;

			inline Set(std::vector<Entry>&& entries);

			std::string to_string() const;
			std::vector<Entry> m_entries;
	};

	struct FieldAccess {
		public:
			FieldAccess() = delete;
			FieldAccess(FieldAccess&) = delete;
			FieldAccess(FieldAccess&&) = default;
			inline FieldAccess(Spanned<Expression>&& lhs, Spanned<std::string>&& rhs);

			std::string to_string() const;

			std::unique_ptr<Spanned<Expression>> m_lhs;
			Spanned<std::string>                 m_rhs;
	};

	struct FunctionCall {
		FunctionCall(FunctionCall&) = delete;
		FunctionCall(FunctionCall&&) = default;

		std::string to_string() const;
		inline FunctionCall(Spanned<Expression>&& function, std::vector<Spanned<Expression>>&& args);

		std::unique_ptr<Spanned<Expression>> m_function;
		std::vector<Spanned<Expression>>     m_args;
	};

	class Expression {
		public:
			static Expression variable(std::string&&);
			static Expression number(std::string&&);
			static Expression string(std::string&&);
			static Expression list(std::vector<Spanned<Expression>>&&);
			static Expression function_call(Spanned<Expression>&& function, std::vector<Spanned<Expression>>&& args);
			static Expression field_access(Spanned<Expression>&& lhs, Spanned<std::string>&& rhs);
			static Expression set(std::vector<Set::Entry>&& entries);

			std::string to_string() const;

			std::optional<std::string_view> as_variable() const;
			std::optional<std::reference_wrapper<const std::string>> as_number() const;
			std::optional<std::string_view> as_string() const;
			OptionalRef<const List> as_list() const;
			OptionalRef<const FunctionCall> as_function_call() const;
			OptionalRef<const Set> as_set() const;

			enum struct Type {
				Variable,
				Number,
				String,
				List,
				FunctionCall,
				FieldAccess,
				Set,
			};

			constexpr Type type() const {
				return m_type;
			}

			Expression(Expression&&);
			~Expression();
		private:
			constexpr Expression() {}

			Type m_type;

			union {
				std::string  m_variable;
				std::string  m_number;
				std::string  m_string;
				List         m_list;
				FunctionCall m_function_call;
				FieldAccess  m_field_access;
				Set          m_set;
			};
	};

	struct Set::Entry {
		Entry(Spanned<std::string>&& key, Spanned<Expression>&& value)
			: key(std::move(key))
			, value(std::move(value)) {}

		Spanned<std::string> key;
		Spanned<Expression>  value;
	};

	inline FunctionCall::FunctionCall(Spanned<Expression>&& function, std::vector<Spanned<Expression>>&& args)
		: m_function(std::make_unique<Spanned<Expression>>(std::move(function)))
		, m_args(std::move(args)) {}

	inline FieldAccess::FieldAccess(Spanned<Expression>&& lhs, Spanned<std::string>&& rhs)
		: m_lhs(std::make_unique<Spanned<Expression>>(std::move(lhs)))
		, m_rhs(std::move(rhs))
		{}

	inline Set::Set(std::vector<Entry>&& entries)
		: m_entries(std::move(entries)){}

	inline List::List(std::vector<Spanned<Expression>>&& elements)
		: m_elements(std::move(elements)) {}
}
