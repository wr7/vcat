#include "src/lexer.hh"
#include "src/error.hh"
#include <format>
#include <optional>
#include <string_view>

using Hint = vcat::Diagnostic::Hint;

namespace vcat {
	std::optional<Spanned<Token>> Lexer::next() {
		for(;;) {
			if(m_remaining_idx >= m_src.length()) {
				return {};
			}

			char c = m_src[m_remaining_idx];

			if(c <= ' ') {
				// Skip whitespace
				m_remaining_idx += 1;
				continue;
			}

			if(c == '#') {
				// Skip comment
				while(m_remaining_idx < m_src.size() && m_src[m_remaining_idx] != '\n') {
					m_remaining_idx++;
				}
				continue;
			}

			auto ident = this->lex_ident_or_number();
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
				if(c != '`') {
					throw Diagnostic(std::format("Error: invalid token `{}`", c), {Hint::error("", Span(m_remaining_idx - 1, 1))});
				} else {
					throw Diagnostic(std::format("Error: invalid token '{}'", c), {Hint::error("", Span(m_remaining_idx - 1, 1))});
				}
		}})();

		return std::optional(Spanned(Token(std::move(tok)), s));
	}

	std::optional<Spanned<Token>> Lexer::lex_ident_or_number() {
		const size_t start = m_remaining_idx;
		size_t len = 0;
		bool is_number = false;

		if(m_remaining_idx < m_src.length()) {
			if(m_src[m_remaining_idx] >= '0' && m_src[m_remaining_idx] <= '9') {
				is_number = true;
			}
		}

		while(m_remaining_idx < m_src.length()) {
			char c = m_src[m_remaining_idx];

			if(
			   (c >= 'a' && c <= 'z')
			|| (c >= 'A' && c <= 'Z')
			|| (c >= '0' && c <= '9')
			||  c == '_'
			|| (is_number && c == '.')
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

		if(is_number) {
			return std::optional(Spanned(Token::number(ident), s));
		} else {
			return std::optional(Spanned(Token::identifier(ident), s));
		}
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
				throw Diagnostic(
					"Error: unclosed string",
					{
						 Hint::info ("String starts here",     Span(start,           1))
						,Hint::error("String is never closed", Span(m_remaining_idx, 0))
					}
				);
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

