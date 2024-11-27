#include "src/eval/eobject.hh"
#include "src/error.hh"
#include "src/eval/error.hh"
#include "src/util.hh"
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>

namespace dvel {
	bool EObject::callable() const {
		return false;
	}

	std::unique_ptr<EObject> EObject::operator()(Spanned<EList &> args) {
		throw eval::error::uncallable_object(*this, args.span);
	}

	void EList::hash(dvel::Hasher& hasher) const {
		hasher.add("_list_");
		const size_t inner_start = hasher.pos();

		for(const Spanned<std::unique_ptr<EObject>>& element : m_elements) {
			element.val->hash(hasher);
		}

		hasher.add((uint64_t) (hasher.pos() - inner_start));
	}

	std::string EList::to_string() const {
		std::stringstream s;
		if(m_elements.size() <= 1) {
			s << "[";
			if(!m_elements.empty()) {
				s << m_elements[0].val->to_string();
			}
			s << "]";

			return s.str();
		}

		s << "[\n";

		for(const Spanned<std::unique_ptr<EObject>>& element : m_elements) {
			s << indent(element.val->to_string()) << ",\n";
		}

		s << "]";

		return s.str();
	}

	std::string EList::type_name() const {
		return "List";
	}

	void EString::hash(dvel::Hasher& hasher) const {
		hasher.add("_string_");
		hasher.add(m_string);
		hasher.add((uint64_t) m_string.size());
	}

	std::string EString::to_string() const {
		std::stringstream s;

		s << '"'
		  << PartiallyEscaped(m_string)
		  << '"';

		return s.str();
	}

	std::string EString::type_name() const {
		return "String";
	}
}
