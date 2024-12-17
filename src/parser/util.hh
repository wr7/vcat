#pragma once

#include "src/error.hh"
#include "src/lexer/token.hh"
#include "src/parser.hh"
#include "src/util.hh"

namespace vcat::parser {
	// Iterates over the tokens in a stream that are not enclosed in brackets.
	class NonBracketed {
		public:
			constexpr NonBracketed(TokenStream tokens)
				: m_remaining(tokens) {}

			OptionalRef<const Spanned<Token>> next();
			OptionalRef<const Spanned<Token>> next_back();
		private:
			TokenStream m_remaining;
	};
}
