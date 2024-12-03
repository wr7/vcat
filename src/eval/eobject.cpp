#include "src/eval/eobject.hh"
#include "src/error.hh"
#include "src/eval/error.hh"
#include "src/util.hh"
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

namespace vcat {
	bool EObject::callable() const {
		return false;
	}

	const EObject& EObject::operator()(EObjectPool&, Spanned<const EList&> args) const {
		throw eval::error::uncallable_object(*this, args.span);
	}

	void EList::hash(vcat::Hasher& hasher) const {
		hasher.add("_list_");
		const size_t inner_start = hasher.pos();

		for(const Spanned<const EObject&>& element : m_elements) {
			element->hash(hasher);
		}

		hasher.add((uint64_t) (hasher.pos() - inner_start));
	}

	std::string EList::to_string() const {
		std::stringstream s;
		if(m_elements.size() <= 1) {
			s << "[";
			if(!m_elements.empty()) {
				s << m_elements[0]->to_string();
			}
			s << "]";

			return s.str();
		}

		s << "[\n";

		for(const Spanned<const EObject&>& element : m_elements) {
			s << indent(element->to_string()) << ",\n";
		}

		s << "]";

		return s.str();
	}

	std::string EList::type_name() const {
		return "List";
	}

	void EString::hash(vcat::Hasher& hasher) const {
		hasher.add("_string_");
		hasher.add(m_string);
		hasher.add((uint64_t) m_string.size());
	}

	std::string EString::to_string() const {
		std::stringstream s;

		s << std::quoted(m_string);

		return s.str();
	}

	std::string EString::type_name() const {
		return "String";
	}
}
