#include "src/error.hh"
#include "src/eval/eobject.hh"
#include "src/eval/eval.hh"
#include "src/lexer.hh"
#include "src/lexer/token.hh"
#include "src/muxing.hh"
#include "src/parser.hh"
#include "src/parser/expression.hh"
#include "src/util.hh"
#include <iostream>
#include <memory>
#include <string_view>

using dvel::Spanned;
using dvel::Token;
using dvel::parser::Expression;

int main() {
	std::string_view input_test = R"--(vopen("prism.mp4"))--";
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

		std::unique_ptr<dvel::EObject> object = dvel::eval::evaluate_expression(expression->as_cref());
		std::cout << object->to_string() << "\n";
		dvel::Hasher hasher;

		object->hash(hasher);

		std::cout << "hash: " << hasher.as_string() << "\n";

		dvel::muxing::write_output(Spanned<dvel::EObject&>(*object.get(), expression->span));

	} catch (dvel::Diagnostic d) {
		std::cout << '\n' << d.render(input_test) << '\n';
	}
}
