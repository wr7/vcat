#pragma once

#include "src/error.hh"
#include "src/lexer/token.hh"

#include <span>

namespace dvel::parser {
	void verify_brackets(std::span<const Spanned<Token>> tokens);
}
