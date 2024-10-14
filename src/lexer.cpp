#include "src/lexer.hh"
#include <format>

namespace dvel {
	std::optional<Spanned<Token>> Lexer::next() {
		start:
		if(m_remaining_idx >= m_src.length()) {
			return std::optional<Spanned<Token>>();
		}

		size_t start_idx = m_remaining_idx;
		size_t len = 0;

		while(m_remaining_idx < m_src.length()) {
			char c = m_src[m_remaining_idx];

			if(c <= ' ') {
				m_remaining_idx += 1;
				break;
			}

			if((c >= 'a' && c <= 'z')
			|| (c >= 'A' && c <= 'Z')
			|| (c >= '0' && c <= '9')
			||  c == '_'
			) {
				m_remaining_idx += 1;
				len += 1;
				continue;
			}

			 switch(c) {
				 case '+':
				 case '-':
				 case '=':
				 case ';':
				 break;

				 default:
				 throw Diagnostic(std::format("Invalid token `{}`", c), {Diagnostic::Hint("", Span(m_remaining_idx, 1))});
			 }

			 if(len == 0) {
				m_remaining_idx += 1;
				len += 1;
			 }
			 break;
		}

		if(len == 0) {
			goto start;
		}

		Token tok = Token(m_src.substr(start_idx, len));

		return std::optional(Spanned(tok, Span(start_idx, len)));
	}

	Token Token::opening(BracketType type) {
		Token t;
		t.type = TokenType::OpeningBracket;
		t.data.bracket_type = type;

		return t;
	}

	Token Token::closing(BracketType type) {
		Token t;
		t.type = TokenType::ClosingBracket;
		t.data.bracket_type = type;

		return t;
	}

	Token Token::other(std::string_view string) {
		Token t;
		t.type = TokenType::Other;
		t.data.other = string;

		return t;
	}
}

