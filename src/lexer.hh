#pragma once

#include "src/error.hh"

#include <optional>
#include <string_view>
#include <format>

namespace dvel {
	enum class BracketType {
		Parenthesis,
		Square,
		Curly,
	};

	class Token {
		public:
			enum struct Type {
				OpeningBracket,
				ClosingBracket,
				Other,
			};

			constexpr Token(std::string_view s);

			Type type() const;

			std::string to_string() const;

			static Token opening(BracketType);
			static Token closing(BracketType);
			static Token other(std::string_view);

			static std::optional<BracketType> as_opening();
			static std::optional<BracketType> as_closing();
			static std::optional<std::string_view> as_other();
		private:
			Type m_type;
			union TokenData {
				std::string_view other;
				BracketType      bracket_type;

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
	};

	constexpr inline Token::Token(std::string_view s) {
		if(s.length() == 1) {
			switch(s[0]) {
				case ')':
				case '(':
					m_data.bracket_type = BracketType::Parenthesis;
					break;
				case ']':
				case '[':
					m_data.bracket_type = BracketType::Square;
					break;
				case '}':
				case '{':
					m_data.bracket_type = BracketType::Curly;
					break;
			}

			switch(s[0]) {
				case '(':
				case '[':
				case '{':
					m_type = Type::OpeningBracket;
					return;
				case ')':
				case ']':
				case '}':
					m_type = Type::OpeningBracket;
					return;
			}
		}

		m_type = Type::Other;
		m_data.other = s;
	};
}
