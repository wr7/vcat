#pragma once

#include <cassert>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace vcat {
	enum struct BracketType {
		Parenthesis,
		Square,
		Curly,
	};

	enum struct SymbolType {
		Dot,
		Comma,
		Semicolon,
		Equals,
	};

	class Token {
		public:
			enum struct Type {
				OpeningBracket,
				ClosingBracket,

				String,
				Number,
				Identifier,

				Symbol,
			};

			consteval Token(std::string_view s);

			Token(const Token& d) = delete;
			Token(Token&& d);
			constexpr ~Token();

			Type type() const;
			bool operator==(const Token& rhs);

			// Displays any `Token` as a string
			std::string to_string() const;

			static Token opening(BracketType);
			static Token closing(BracketType);
			static Token identifier(std::string_view);
			static Token number(std::string_view);
			static Token string(std::string&&);

			std::optional<BracketType> as_opening() const;
			std::optional<BracketType> as_closing() const;

			std::optional<std::string_view> as_string() const;
			std::optional<std::string_view> as_identifier() const;
			std::optional<std::string_view> as_number() const;

			std::optional<SymbolType> as_symbol() const;
		private:
			Type m_type;
			union {
				std::string_view m_identifier;
				std::string_view m_number;
				std::string      m_string;
				BracketType      m_bracket_type;

				SymbolType       m_symbol;
			};

			inline Token() noexcept {};
	};

	constexpr std::optional<SymbolType> SymbolType_from_string(std::string_view s) {
		if(s.length() != 1) {
			return std::optional<SymbolType>();
		}

		SymbolType ty;

		switch(s[0]) {
			case '.':
				ty = SymbolType::Dot;       break;
			case ',':
				ty = SymbolType::Comma;     break;
			case ';':
				ty = SymbolType::Semicolon; break;
			case '=':
				ty = SymbolType::Equals;    break;
			default:
				return std::optional<SymbolType>();
		}

		return std::optional(ty);
	}

	consteval Token::Token(std::string_view s) {
		assert(s.length() > 0);

		if(s.length() == 1) {
			switch(s[0]) {
				case '(': case ')':
					m_bracket_type = BracketType::Parenthesis;
					break;
				case '[': case ']':
					m_bracket_type = BracketType::Square;
					break;
				case '{': case '}':
					m_bracket_type = BracketType::Curly;
					break;
			}

			switch(s[0]) {
				case '(': case '[': case '{':
					m_type = Type::OpeningBracket;
					return;
				case ')': case ']': case '}':
					m_type = Type::ClosingBracket;
					return;
			}
		}

		auto symbol_type = SymbolType_from_string(s);
		if(symbol_type.has_value()) {
			m_type = Type::Symbol;
			m_symbol = symbol_type.value();
			return;
		}

		if(s[0] >= '0' && s[0] <= '9') {
			m_type = Type::Number;
			m_number = s;
			return;
		}

		m_type = Type::Identifier;
		std::construct_at(&m_identifier, s);
	};

	constexpr Token::~Token() {
		switch(m_type) {
			case Type::OpeningBracket:
			case Type::ClosingBracket:
			case Type::Identifier:
			case Type::Number:
			case Type::Symbol:
				break; // No destructor needed
			case Type::String:
				m_string.std::string::~string();
				break;
		}
	}
}
