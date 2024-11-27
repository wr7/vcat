#pragma once

#include "src/error.hh"
#include "src/util.hh"

#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace dvel {
	class EObject;
	class EList;

	using builtin_function = std::unique_ptr<EObject> (*)(Spanned<EList&> args);

	class EObject {
		public:
			// Adds the object to a hasher
			//
			// NOTE: To prevent hash collisions, every object should be of the following form:
			// 1. An underscore
			// 2. Some unique type identifier (that does not contain underscores)
			// 3. Another underscore
			// 4. Data that uniquely represents a particular instance of the type
			// 5. The size of the above data as a big endian 64 bit integer
			virtual void hash(dvel::Hasher& hasher) const = 0;
			virtual std::string to_string() const = 0;
			virtual std::string type_name() const = 0;

			virtual bool callable() const;
			virtual std::unique_ptr<EObject> operator()(Spanned<EList&> args);
	};
	static_assert(std::is_abstract<EObject>());

	template<builtin_function f, StringLiteral name>
	class BuiltinFunction : public EObject {
		public:
			inline void hash(dvel::Hasher& hasher) const {
				constexpr StringLiteral prefix = "_builtin_" + name;
				hasher.add(*prefix);
				hasher.add((uint64_t) name.size());
			}

			inline std::string to_string() const {
				constexpr StringLiteral s = "builtin(" + name + ")";
				return std::string(*s);
			}

			inline std::string type_name() const {
				constexpr StringLiteral tname = "builtin_" + name;

				return std::string(*tname);
			}

			inline bool callable() const {
				return true;
			}

			std::unique_ptr<EObject> operator()(Spanned<EList&> args) {
				return f(args);
			}
	};

	class EList : public EObject {
		public:
			void hash(dvel::Hasher& hasher) const;
			std::string to_string() const;
			std::string type_name() const;

			constexpr EList(std::vector<Spanned<std::unique_ptr<EObject>>>&& elements)
				: m_elements(std::move(elements)) {}

			constexpr std::span<const Spanned<std::unique_ptr<EObject>>> elements() const {
				return m_elements;
			}

		private:
			std::vector<Spanned<std::unique_ptr<EObject>>> m_elements;
	};
	static_assert(!std::is_abstract<EList>());

	class EString : public EObject {
		public:
			void hash(dvel::Hasher& hasher) const;
			std::string to_string() const;
			std::string type_name() const;

			constexpr EString(std::string&& s)
				: m_string(std::move(s)) {}

			constexpr std::string_view operator*() const {
				return m_string;
			}

		private:
			std::string m_string;
	};
	static_assert(!std::is_abstract<EString>());
}

