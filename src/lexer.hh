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

	enum struct TokenType {
		OpeningBracket,
		ClosingBracket,
		Other,
	};

	class Token {
		public:
			constexpr Token(std::string_view s);

			static Token opening(BracketType);
			static Token closing(BracketType);
			static Token other(std::string_view);

			static std::optional<BracketType> as_opening();
			static std::optional<BracketType> as_closing();
			static std::optional<std::string_view> as_other();
		private:
			TokenType type;
			union TokenData {
				std::string_view other;
				BracketType      bracket_type;

				constexpr TokenData() {}
			} data;

			inline Token() {}
	};

	class Lexer {
		public:
			constexpr inline Lexer(std::string_view src): m_src(src), m_remaining_idx(0) {}
			std::optional<Spanned<Token>> next();
		private:
			std::string_view m_src;
			size_t m_remaining_idx;
	};

	constexpr inline Token::Token(std::string_view s) {
		if(s.length() == 1) {
			switch(s[0]) {
				case ')':
				case '(':
					this->data.bracket_type = BracketType::Parenthesis;
					break;
				case ']':
				case '[':
					this->data.bracket_type = BracketType::Square;
					break;
				case '}':
				case '{':
					this->data.bracket_type = BracketType::Curly;
					break;
			}

			switch(s[0]) {
				case '(':
				case '[':
				case '{':
					this->type = TokenType::OpeningBracket;
					return;
				case ')':
				case ']':
				case '}':
					this->type = TokenType::OpeningBracket;
					return;
			}
		}

		this->type = TokenType::Other;
		this->data.other = s;
	};
}
