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
	std::vector<Expression> exprs;
	exprs.push_back(Expression::variable("cool_video.mp4"));
	exprs.push_back(Expression::variable("other_video.mp4"));

	Expression encode = Expression::function_call(Expression::variable("encode"), std::move(exprs));

	exprs.push_back(Expression::variable("foo"));
	exprs.push_back(std::move(encode));
	exprs.push_back(Expression::variable("bar"));

	Expression e = Expression::set(std::move(exprs));

	std::cout << e.to_string() << '\n';

	std::string_view input_test =
R"--(let edited = v.encode(videos["\"test\" video.mp4"]);
    let z = 7;
)--";
	dvel::Lexer lexer = dvel::Lexer(input_test);

	std::vector<Spanned<Token>> tokens;
	try {
		while(auto token = lexer.next()) {
			tokens.push_back(std::move(*token));
		}

		dvel::parser::verify_brackets(tokens);
	} catch (dvel::Diagnostic d) {
		std::cout << '\n' << d.render(input_test) << '\n';
	}
}
