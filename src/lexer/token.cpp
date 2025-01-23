#include "src/lexer/token.hh"

#include <format>
#include <iomanip>
#include <optional>
#include <sstream>
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

	static std::string_view BracketType_closing(BracketType ty) {
		switch(ty) {
			case BracketType::Parenthesis:
				return ")";
			case BracketType::Square:
				return "]";
			case BracketType::Curly:
				return "}";
		}

		throw; // unreachable
	}

	static std::string_view BracketType_opening(BracketType ty) {
		switch(ty) {
			case BracketType::Parenthesis:
				return "(";
			case BracketType::Square:
				return "[";
			case BracketType::Curly:
				return "{";
		}

		throw; // unreachable
	}

	std::string Token::to_string() const {
		switch(m_type) {
			case Type::OpeningBracket:
				return std::format("{}", BracketType_opening(m_bracket_type));
			case Type::ClosingBracket:
				return std::format("{}", BracketType_closing(m_bracket_type));
			case Type::Identifier:
				return std::format("{}", m_identifier);
			case Type::Number:
				return std::format("{}", m_number);
			case Type::String:
				return (std::stringstream() << std::quoted(m_string)).str();
			case Type::Symbol:
				return std::format("{}", SymbolType_to_str(m_symbol));
		}

		std::abort(); // unreachable
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

	Token Token::number(std::string_view number) {
		Token t;
		t.m_type = Type::Number;
		t.m_number = number;

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

	std::optional<std::string_view> Token::as_number() const {
		if(m_type != Type::Number) {
			return std::optional<std::string_view>();
		}

		return m_number;
	}

	std::optional<SymbolType> Token::as_symbol() const {
		if(m_type != Type::Symbol) {
			return std::optional<SymbolType>();
		}

		return m_symbol;
	}

	Token::Token(Token &&d) {
		m_type = d.m_type;

		switch(d.m_type) {
			case Type::OpeningBracket:
			case Type::ClosingBracket:
				m_bracket_type = d.m_bracket_type;
				return;
			case Type::String:
				new(&m_string) std::string(std::move(d.m_string));
				return;
			case Type::Identifier:
				m_identifier = d.m_identifier;
				return;
			case Type::Number:
				m_number = d.m_number;
				return;
			case Type::Symbol:
				m_symbol = d.m_symbol;
				return;
		}

		std::abort(); // unreachable
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
			case Type::Number:
				return m_number == rhs.m_number;
			case Type::Symbol:
				return m_symbol == rhs.m_symbol;
		}

		std::abort(); // unreachable
	}
}
