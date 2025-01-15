#include "src/eval/scope.hh"
#include <ranges>
#include <tuple>

namespace vcat::eval {
	void Scope::add(std::string&& var_name, Spanned<const EObject&> value) {
		std::tuple<std::string, Spanned<const EObject&>> entry = {std::move(var_name), value};

		m_variables.push_back(std::move(entry));
	}

	std::optional<Spanned<const EObject&>> Scope::get(std::string_view var_name) const {
		for(const auto& var : std::ranges::reverse_view(m_variables)) {
			auto name = std::get<0>(var);
			Spanned<const EObject&> val = std::get<1>(var);

			if(name == var_name) {
				return val;
			}
		}

		if(!m_parent) {
			return {};
		}

		return m_parent->get().get(var_name);
	}
}
