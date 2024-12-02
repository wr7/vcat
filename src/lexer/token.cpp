#include "src/lexer/token.hh"

#include <format>
#include <optional>
#include <string_view>

namespace vcat {
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

		throw; // unreachable
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
		new(&t.m_string) std::string(std::move(string));

		return Token(std::move(t));
	}

	std::optional<BracketType> Token::as_opening() const {
		if(m_type != Type::OpeningBracket) {
			return std::optional<BracketType>();
		}

		return m_bracket_type;
	}

	std::optional<BracketType> Token::as_closing() const {
		if(m_type != Type::ClosingBracket) {
			return std::optional<BracketType>();
		}

		return m_bracket_type;
	}

	std::optional<std::string_view> Token::as_string() const {
		if(m_type != Type::String) {
			return std::optional<std::string_view>();
		}

		return std::string_view(m_string);
	}

	std::optional<std::string_view> Token::as_identifier() const {
		if(m_type != Type::Identifier) {
			return std::optional<std::string_view>();
		}

		return m_identifier;
	}

	std::optional<SymbolType> Token::as_symbol() const {
		if(m_type != Type::Symbol) {
			return std::optional<SymbolType>();
		}

		return m_symbol;
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
	}

	bool Token::operator==(const Token& rhs) {
		if(m_type != rhs.m_type) {
			return false;
		}

		switch(m_type) {
			case Type::OpeningBracket:
			case Type::ClosingBracket:
				return m_bracket_type == rhs.m_bracket_type;
			case Type::String:
				return m_string == rhs.m_string;
			case Type::Identifier:
				return m_identifier == rhs.m_identifier;
			case Type::Symbol:
				return m_symbol == rhs.m_symbol;
		}

		std::abort(); // unreachable
	}
}
