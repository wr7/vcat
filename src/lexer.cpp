#include "src/lexer.hh"
#include "src/error.hh"
#include <format>
#include <optional>
#include <string_view>

namespace dvel {
	std::optional<Spanned<Token>> Lexer::next() {
		for(;;) {
			if(m_remaining_idx >= m_src.length()) {
				return std::optional<Spanned<Token>>();
			}

			char c = m_src[m_remaining_idx];

			if(c <= ' ') {
				m_remaining_idx += 1;
				continue;
			}

			auto ident = this->lex_ident();
			if(ident.has_value()){
				return ident;
			}

			auto string = this->lex_string();
			if(string.has_value()){
				return string;
			}

			auto symbol = this->lex_symbol();
			if(symbol.has_value()) {
				return symbol;
			}
		}
	}

	std::optional<Spanned<Token>> Lexer::lex_symbol() {
		if(m_remaining_idx >= m_src.length()) {
			return std::optional<Spanned<Token>>();
		}

		char c = m_src[m_remaining_idx];
		Span s = Span(m_remaining_idx, 1);
		m_remaining_idx += 1;

		Token tok = ([=, this]() {switch(c) {
			case '.': return Token(".");
			case ',': return Token(",");
			case ';': return Token(";");
			case '=': return Token("=");
			case '(': return Token("(");
			case ')': return Token(")");
			case '[': return Token("[");
			case ']': return Token("]");
			case '{': return Token("{");
			case '}': return Token("}");
			default:
				throw Diagnostic(std::format("Invalid token `{}`", c), {Diagnostic::Hint("", Span(m_remaining_idx, 1))});
		}})();

		return std::optional(Spanned(Token(std::move(tok)), s));
	}

	std::optional<Spanned<Token>> Lexer::lex_ident() {
		const size_t start = m_remaining_idx;
		size_t len = 0;

		while(m_remaining_idx < m_src.length()) {
			char c = m_src[m_remaining_idx];

			if(
			   (c >= 'a' && c <= 'z')
			|| (c >= 'A' && c <= 'Z')
			|| (c >= '0' && c <= '9')
			||  c == '_'
			) {
				len += 1;
				m_remaining_idx += 1;
				continue;
			} else {
				break;
			}
		}

		if(len == 0) {
			return std::optional<Spanned<Token>>();
		}

		Span s = Span(start, len);
		std::string_view ident = m_src.substr(start, len);

		return std::optional(Spanned(Token::identifier(ident), s));
	}

	std::optional<Spanned<Token>> Lexer::lex_string() {
		if(m_remaining_idx >= m_src.length()) {
			return std::optional<Spanned<Token>>();
		}

		char c = m_src[m_remaining_idx];


		if(c != '"') {
			return std::optional<Spanned<Token>>();
		}

		const size_t start = m_remaining_idx;
		m_remaining_idx += 1;
		bool escape = false;
		std::string string;

		for(;;) {
			if(m_remaining_idx >= m_src.length()) {
				throw Diagnostic("Expected `\"`; got EOF", {Diagnostic::Hint("", Span(m_remaining_idx, 0))});
			}

			char c = m_src[m_remaining_idx];
			m_remaining_idx += 1;

			if(!escape) {
				if(c == '"') {
					break;
				}

				if(c == '\\') {
					escape = true;
					continue;
				}
			}

			escape = false;
			string += c;
		}

		Span s = Span(start, m_remaining_idx - start);
		Token t = Token::string(std::move(string));

		return Spanned(std::move(t), s);
	}

}

