#include "src/error.hh"
#include "src/lexer.hh"
#include <iostream>
#include <string_view>

int main() {
	std::string_view input_test = "let edited = v.encode(vids[\"cool video.mp4\"])";
	dvel::Lexer lexer = dvel::Lexer(input_test);

	try {
		for(;;) {
			auto token_ = lexer.next();
			if(!token_.has_value()) {
				break;
			}

			dvel::Spanned<dvel::Token> token = std::move(token_.value());

			std::cout << token.val.to_string() << '\n';
		}
	}
	catch(dvel::Diagnostic d) {
		std::cout << d.msg() << '\n';
	}
}

