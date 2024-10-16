#pragma once

#include "src/error.hh"

#include <optional>
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

			Type type() const;

			std::string to_string() const;

			static Token opening(BracketType);
			static Token closing(BracketType);
			static Token identifier(std::string_view);

			static std::optional<BracketType> as_opening();
			static std::optional<BracketType> as_closing();
			static std::optional<std::string_view> as_other();
		private:
			Type m_type;
			union TokenData {
				std::string_view identifier;
				std::string_view string;
				BracketType      bracket_type;

				SymbolType       symbol;

				constexpr TokenData() {}
			} m_data;

			inline Token() {}
	};

	class Lexer {
		public:
			constexpr inline Lexer(std::string_view src): m_src(src), m_remaining_idx(0) {}
			std::optional<Spanned<Token>> next();
		private:
			std::string_view m_src;
			size_t m_remaining_idx;

			std::optional<Spanned<Token>> lex_symbol();
			std::optional<Spanned<Token>> lex_ident();
	};

	constexpr std::optional<SymbolType> SymbolType_from_string(std::string_view s) {
		if(s.length() != 1) {
			return std::optional<SymbolType>();
		}

		SymbolType ty;

		switch(s[0]) {
			case '.':
				ty = SymbolType::Dot;
				break;
			case ',':
				ty = SymbolType::Comma;
				break;
			case ';':
				ty = SymbolType::Semicolon;
				break;
			case '=':
				ty = SymbolType::Equals;
				break;
			default:
				return std::optional<SymbolType>();
		}

		return std::optional(ty);
	}

	consteval Token::Token(std::string_view s) {
		if(s.length() == 1) {
			switch(s[0]) {
				case '(': case ')':
					m_data.bracket_type = BracketType::Parenthesis;
					break;
				case '[': case ']':
					m_data.bracket_type = BracketType::Square;
					break;
				case '{': case '}':
					m_data.bracket_type = BracketType::Curly;
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
			m_data.symbol = symbol_type.value();
			return;
		}

		m_type = Type::Identifier;
		m_data.identifier = s;
	};
}
