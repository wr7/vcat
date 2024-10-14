#include "src/lexer.hh"
#include <format>

namespace dvel {
	std::optional<Spanned<Token>> Lexer::next() {
		start:
		if(m_remaining_idx >= m_src.length()) {
			return std::optional<Spanned<Token>>();
		}

		const size_t start_idx = m_remaining_idx;
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
				case '+': case '-':
				case '=': case ';':
				case '.': case ',':
				case '{': case '}':
				case '[': case ']':
				case '(': case ')':
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

	static std::string_view BracketType_to_str(BracketType ty) {
		switch(ty) {
			case BracketType::Parenthesis:
				return "Parenthesis";
			case BracketType::Square:
				return "Square";
			case BracketType::Curly:
				return "Curly";
		}

		throw;
	}

	std::string Token::to_string() const {
		switch(m_type) {
			case Type::OpeningBracket:
				return std::format("OpeningBracket({})", BracketType_to_str(m_data.bracket_type));
			case Type::ClosingBracket:
				return std::format("ClosingBracket({})", BracketType_to_str(m_data.bracket_type));
			case Type::Other:
				return std::format("Other({})", m_data.other);
		}
	}

	Token Token::opening(BracketType type) {
		Token t;
		t.m_type = Type::OpeningBracket;
		t.m_data.bracket_type = type;

		return t;
	}

	Token Token::closing(BracketType type) {
		Token t;
		t.m_type = Type::ClosingBracket;
		t.m_data.bracket_type = type;

		return t;
	}

	Token Token::other(std::string_view string) {
		Token t;
		t.m_type = Type::Other;
		t.m_data.other = string;

		return t;
	}
}

