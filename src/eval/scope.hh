#pragma once

#include "src/error.hh"
#include "src/util.hh"

#include <string>
#include <string_view>
#include <tuple>

namespace vcat {
	class EObject;
}

namespace vcat::eval {
	class Scope {
		public:
			/// Creates a global scope
			constexpr Scope() {}

			constexpr Scope(const Scope& parent)
				: m_parent(parent)
			{}

			void add(std::string&& var_name, Spanned<const EObject&> value);
			std::optional<Spanned<const EObject&>> get(std::string_view var_name) const;

		private:
			std::vector<std::tuple<std::string, Spanned<const EObject&>>> m_variables;
			OptionalRef<const Scope> m_parent;
	};
}
