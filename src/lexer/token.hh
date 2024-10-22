#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace dvel {
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
				Identifier,

				Symbol,
			};

			consteval Token(std::string_view s);

			Token(const Token& d) = delete;
			Token(Token&& d);
			~Token();

			Type type() const;
			bool operator==(const Token& rhs);

			// Displays any `Token` as a string
			std::string to_string() const;

			static Token opening(BracketType);
			static Token closing(BracketType);
			static Token identifier(std::string_view);
			static Token string(std::string&& string);

			std::optional<BracketType> as_opening() const;
			std::optional<BracketType> as_closing() const;

			std::optional<std::string_view> as_string() const;
			std::optional<std::string_view> as_identifier() const;

			std::optional<SymbolType> as_symbol() const;
		private:
			Type m_type;
			union {
				std::string_view m_identifier;
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

		m_type = Type::Identifier;
		m_identifier = s;
	};
}
