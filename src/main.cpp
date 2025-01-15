#include "src/error.hh"
#include "src/eval/eobject.hh"
#include "src/eval/eval.hh"
#include "src/eval/scope.hh"
#include "src/lexer.hh"
#include "src/lexer/token.hh"
#include "src/muxing.hh"
#include "src/parser.hh"
#include "src/parser/expression.hh"
#include "src/util.hh"
#include "src/shared.hh"

#include <iostream>
#include <string_view>

using vcat::Spanned;
using vcat::Token;
using vcat::parser::Expression;

// TODO:
// - Audio support
// - Variables
// - Lambdas
// - Attempt to re-use the same encoder
// - Fixed frame rate support
// - Fix token printing in error messages

int main() {
	const shared::Parameters params = shared::vcat_cli_parse();

	std::string_view expression((char *) params.expression.data, params.expression.capacity);

	vcat::Lexer lexer = vcat::Lexer(expression);

	std::vector<Spanned<Token>> tokens;
	try {
		while(auto token = lexer.next()) {
			tokens.push_back(std::move(*token));
		}

		vcat::parser::verify_brackets(tokens);
		std::optional<Spanned<Expression>> expression = vcat::parser::try_parse_expression(tokens);

		if(!expression.has_value()) {
			std::cout << "empty expression!" << "\n";
			return 0;
		}

		vcat::EObjectPool pool;
		vcat::eval::Scope scope;

		const vcat::EObject& object = vcat::eval::evaluate_expression(pool, scope, expression->as_cref());
		std::cout << object.to_string() << "\n";
		vcat::Hasher hasher;

		object.hash(hasher);

		std::cout << "hash: " << hasher.into_string() << "\n";

		vcat::muxing::write_output(Spanned<const vcat::EObject&>(object, expression->span), params);

	} catch (vcat::Diagnostic d) {
		std::cout << '\n' << d.render(expression) << '\n';
	}
}
