#include "src/eval/eobject.hh"
#include "sha256.h"
#include "src/error.hh"
#include "src/eval/error.hh"
#include "src/util.hh"
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <format>
#include <fstream>
#include <ios>
#include <memory>
#include <sstream>
#include <string>

namespace dvel {
	bool EObject::callable() const {
		return false;
	}

	std::unique_ptr<EObject> EObject::operator()(Spanned<EListRef> args) {
		throw eval::error::uncallable_object(*this, args.span);
	}

	void VideoFile::hash(Hasher& hasher) const {
		hasher.add("_videofile_");
		hasher.add(&m_file_hash, sizeof(m_file_hash));
		hasher.add((uint64_t) sizeof(m_file_hash));
	}

	// NOTE: throws `std::string` upon IO failure
	VideoFile::VideoFile(std::string&& path) : m_path(std::move(path)) {
		std::ifstream f;
		f.open(m_path, std::ios_base::in | std::ios_base::binary);

		char buf[512];

		f.rdbuf()->pubsetbuf(0, 0);

		SHA256 hasher;

		while(f.good()) {
			size_t num_bytes = f.read(&buf[0], sizeof(buf)).gcount();

			hasher.add(&buf[0], num_bytes);
		}

		hasher.getHash(m_file_hash);

		if(f.fail()) {
			throw std::format("Failed to open file `{}`: {}", path, strerror(errno));
		}

		f.close();
	}

	std::string VideoFile::to_string() const {
		std::stringstream s;
		s << "VideoFile(\""
			<< PartiallyEscaped(m_path)
			<< "\")";

		return s.str();
	}

	std::string VideoFile::type_name() const {
		return "VideoFile";
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
