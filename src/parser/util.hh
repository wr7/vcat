#pragma once

#include "src/error.hh"
#include "src/lexer/token.hh"
#include "src/parser.hh"
#include "src/util.hh"

namespace vcat::parser {
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
