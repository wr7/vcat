#include "src/error.hh"
#include "src/lexer.hh"
#include <iostream>
#include <string_view>

int main() {
	std::string_view input_test = "let x = y({z = [w(), f()]})";
	dvel::Lexer lexer = dvel::Lexer(input_test);

	for(;;) {
		auto token_ = lexer.next();
		if(!token_.has_value()) {
			break;
		}

		dvel::Spanned<dvel::Token> token = token_.value();

		std::cout << token.val.to_string() << std::endl;
	}
}

