#include "src/error.hh"
#include "src/lexer.hh"
#include "src/lexer/token.hh"
#include "src/parser.hh"
#include <iostream>
#include <string_view>

using dvel::Spanned;
using dvel::Token;

int main() {
	std::string_view input_test =
R"--(let edited = v.encode(videos["\"test\" video.mp4"]);
let y = [
    let z = 7;
}
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
