#include "src/error.hh"
#include "src/lexer.hh"
#include "src/lexer/token.hh"
#include "src/parser.hh"
#include "src/parser/expression.hh"
#include <iostream>
#include <string_view>

using dvel::Spanned;
using dvel::Token;
using dvel::parser::Expression;

int main() {
	std::string_view input_test = R"--([videos, v.process(cool_video, foo(), "abba"), [other1, other2], other3])--";
	dvel::Lexer lexer = dvel::Lexer(input_test);

	std::vector<Spanned<Token>> tokens;
	try {
		while(auto token = lexer.next()) {
			tokens.push_back(std::move(*token));
		}

		dvel::parser::verify_brackets(tokens);
		std::optional<Spanned<Expression>> expression = dvel::parser::try_parse_expression(tokens);

		if(!expression.has_value()) {
			std::cout << "empty expression!" << "\n";
			return 0;
		}

		std::cout << expression->val.to_string() << "\n";
	} catch (dvel::Diagnostic d) {
		std::cout << '\n' << d.render(input_test) << '\n';
	}
}
