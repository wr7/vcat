#pragma once

#include "src/error.hh"
#include "src/lexer/token.hh"

#include <cstddef>
#include <cstring>
#include <optional>
#include <string_view>

namespace vcat {
	class Lexer {
		public:
			constexpr inline Lexer(std::string_view src)
				: m_src(src)
				, m_remaining_idx(0)
			{}

			std::optional<Spanned<Token>> next();
		private:
			std::string_view m_src;
			size_t m_remaining_idx;

			std::optional<Spanned<Token>> lex_symbol();
			std::optional<Spanned<Token>> lex_ident_or_number();
			std::optional<Spanned<Token>> lex_string();
			void lex_comment();
	};
}
