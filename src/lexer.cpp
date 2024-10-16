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

	static std::string_view SymbolType_to_str(SymbolType ty) {
		switch(ty) {
			case SymbolType::Dot:
				return ".";
			case SymbolType::Comma:
				return ",";
			case SymbolType::Semicolon:
				return ";";
			case SymbolType::Equals:
				return "=";
		}

		throw; // unreachable
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
				return std::format("OpeningBracket({})", BracketType_to_str(m_bracket_type));
			case Type::ClosingBracket:
				return std::format("ClosingBracket({})", BracketType_to_str(m_bracket_type));
			case Type::Identifier:
				return std::format("Identifier({})", m_identifier);
			case Type::String:
				return std::format("String({})", m_string);
			case Type::Symbol:
				return std::format("Symbol({})", SymbolType_to_str(m_symbol));
		}

		throw; // unreachable
	}

	Token Token::opening(BracketType type) {
		Token t;
		t.m_type = Type::OpeningBracket;
		t.m_bracket_type = type;

		return Token(std::move(t));
	}

	Token Token::closing(BracketType type) {
		Token t;
		t.m_type = Type::ClosingBracket;
		t.m_bracket_type = type;

		return Token(std::move(t));
	}

	Token Token::identifier(std::string_view identifier) {
		Token t;
		t.m_type = Type::Identifier;
		t.m_identifier = identifier;

		return Token(std::move(t));
	}

	Token Token::string(std::string&& string) {
		Token t;
		t.m_type = Type::String;
		new(&t.m_string) std::string(string);

		return Token(std::move(t));
	}

	Token::~Token() {
		switch(m_type) {
			case Type::OpeningBracket:
			case Type::ClosingBracket:
			case Type::Identifier:
			case Type::Symbol:
				break; // No destructor needed
			case Type::String:
				m_string.std::string::~string();
				break;
		}
	}

	Token::Token(Token &&d) {
		m_type = d.m_type;

		switch(d.m_type) {
			case Type::OpeningBracket:
			case Type::ClosingBracket:
				m_bracket_type = d.m_bracket_type;
				break;
			case Type::String:
				new(&m_string) std::string(std::move(d.m_string));
				break;
			case Type::Identifier:
				m_identifier = d.m_identifier;
				break;
			case Type::Symbol:
				m_symbol = d.m_symbol;
				break;
		}

		// To prevent the destructor of `d` from doing anything //
		d.m_type = Type::Symbol;
		d.m_symbol = SymbolType::Dot;
	}
}

