#include "src/error.hh"
#include "src/lexer.hh"
#include "src/lexer/token.hh"
#include <iostream>
#include <string_view>

int main() {
	std::string_view input_test = 
R"--(let edited = v.encode(videos["\"test\" video.mp4"])
let z = 7;
let w = ";
foo bar
)--";
	dvel::Lexer lexer = dvel::Lexer(input_test);

	try {
		for(;;) {
			lexer.next();
		}
	}
	catch(dvel::Diagnostic d) {
		std::cout << '\n' << d.render(input_test) << '\n';
	}
}
