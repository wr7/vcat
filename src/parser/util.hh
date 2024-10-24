#pragma once

#include "src/error.hh"
#include "src/lexer/token.hh"
#include "src/parser.hh"

#include <cstddef>
#include <iterator>
#include <ranges>

namespace dvel::parser {
	// Iterator over `NonBracketed` class
	class NonBracketedIter {
		private:
			const Spanned<Token> *m_ptr;
			bool                  m_nonexistant;

		public:
			using difference_type = std::ptrdiff_t;
			using value_type      = const Spanned<Token> *;

			constexpr NonBracketedIter(const Spanned<Token> *ptr, bool nonexistant)
				: m_ptr(ptr)
				, m_nonexistant(nonexistant) {}

			constexpr bool operator==(const NonBracketedIter& rhs) const {return m_ptr == rhs.m_ptr;}

			constexpr const Spanned<Token> *operator*() const {
				return m_ptr;
			}

			NonBracketedIter &operator++();
			NonBracketedIter &operator--();

			// It is undefined behavior to dereference of increment the returned object, but this constructor
			// is required for `std::bidirectional_iterator`
			constexpr NonBracketedIter(): m_ptr(nullptr), m_nonexistant(true) {}

			inline NonBracketedIter operator++(int) {
				NonBracketedIter retval = *this;
				++*this;
				return retval;
			}
			inline NonBracketedIter operator--(int) {
				NonBracketedIter retval = *this;
				--*this;
				return retval;
			}
	};

	static_assert(std::bidirectional_iterator<NonBracketedIter>);

	// A range of the tokens in a `TokenStream` that aren't enclosed in brackets.
	class NonBracketed : public std::ranges::view_base {
		public:
			constexpr NonBracketed(TokenStream tokens)
				: m_tokens(tokens) {}

			constexpr NonBracketedIter begin() const {
				return NonBracketedIter(m_tokens.data(), false);
			}
			constexpr NonBracketedIter end() const {
				return NonBracketedIter(m_tokens.data() + m_tokens.size(), true);
			}
			constexpr NonBracketedIter rbegin() const {
				return NonBracketedIter(m_tokens.data() - 1, true);
			}
			constexpr NonBracketedIter rend() const {
				return NonBracketedIter(m_tokens.data() + m_tokens.size() - 1, false);
			}
		private:
			TokenStream m_tokens;
	};

	static_assert(std::ranges::view<NonBracketed>);
	static_assert(std::ranges::bidirectional_range<NonBracketed>);
}
