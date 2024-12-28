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
#include <string_view>

using vcat::Spanned;
using vcat::Token;
using vcat::parser::Expression;

extern "C" void rust_hello();

// TODO:
// - support user-specified codecs
// - support multiple streams with concat filter
// - automatically re-encode when using concat filter
// - calculate correct timestamp information with transcoded streams

int main() {
	rust_hello();

	std::string_view input_test = R"--(
    concat(
        vopen("prism2.mp4"),
        vopen("prism.mp4"),
    )
	)--";
	vcat::Lexer lexer = vcat::Lexer(input_test);

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
		const vcat::EObject& object = vcat::eval::evaluate_expression(pool, expression->as_cref());
		std::cout << object.to_string() << "\n";
		vcat::Hasher hasher;

		object.hash(hasher);

		std::cout << "hash: " << hasher.into_string() << "\n";

		vcat::muxing::write_output(Spanned<const vcat::EObject&>(object, expression->span));

	} catch (vcat::Diagnostic d) {
		std::cout << '\n' << d.render(input_test) << '\n';
	}
}
