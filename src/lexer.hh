#pragma once

#include "src/error.hh"
#include "src/lexer/token.hh"

#include <cstddef>
#include <cstring>
#include <optional>
#include <string_view>

namespace dvel {
	class Lexer {
		public:
			constexpr inline Lexer(std::string_view src): m_src(src), m_remaining_idx(0) {}
			std::optional<Spanned<Token>> next();
		private:
			std::string_view m_src;
			size_t m_remaining_idx;

			std::optional<Spanned<Token>> lex_symbol();
			std::optional<Spanned<Token>> lex_ident();
			std::optional<Spanned<Token>> lex_string();
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
