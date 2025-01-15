#include "src/eval/scope.hh"
#include "src/util.hh"
#include <ranges>
#include <tuple>

namespace vcat::eval {
	void Scope::add(std::string&& var_name, const EObject& value) {
		std::tuple<std::string, const EObject&> entry = {std::move(var_name), value};

		m_variables.push_back(std::move(entry));
	}

	OptionalRef<const EObject> Scope::get(std::string_view var_name) const {
		for(const auto& var : std::ranges::reverse_view(m_variables)) {
			auto name = std::get<0>(var);
			const EObject& val = std::get<1>(var);

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
