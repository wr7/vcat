#pragma once

#include "src/error.hh"
#include "src/util.hh"

#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace vcat {
	class EObjectPool;
	class EObject;
	class EList;

	using builtin_function = const EObject& (*)(EObjectPool& pool, Spanned<const EList&> args);

	class EObjectPool {
		public:
			template<typename T, class... Args>
			T& add(Args&&... args) {
				std::unique_ptr<T> ptr = std::make_unique<T>(std::forward<Args>(args)...);
				m_objects.push_back(std::move(ptr));

				return dynamic_cast<T&>(*m_objects.back());
			}

			template<typename T>
			T& ptr(std::unique_ptr<T>&& ptr) {
				m_objects.push_back(std::move(ptr));

				return dynamic_cast<T&>(*m_objects.back());
			}
		private:
			std::vector<std::unique_ptr<EObject>> m_objects;
	};

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
			virtual void hash(vcat::Hasher& hasher) const = 0;
			virtual std::string to_string() const = 0;
			virtual std::string type_name() const = 0;

			virtual bool callable() const;
			virtual const EObject& operator()(EObjectPool& pool, Spanned<const EList&> args) const;
	};
	static_assert(std::is_abstract<EObject>());

	template<builtin_function f, StringLiteral name>
	class BuiltinFunction : public EObject {
		public:
			inline void hash(vcat::Hasher& hasher) const {
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

			const EObject& operator()(EObjectPool& pool, Spanned<const EList&> args) const {
				return f(pool, args);
			}
	};

	class EList : public EObject {
		public:
			void hash(vcat::Hasher& hasher) const;
			std::string to_string() const;
			std::string type_name() const;

			constexpr EList(std::vector<Spanned<const EObject&>>&& elements)
				: m_elements(std::move(elements)) {}

			constexpr std::span<const Spanned<const EObject&>> elements() const {
				return m_elements;
			}

		private:
			std::vector<Spanned<const EObject&>> m_elements;
	};
	static_assert(!std::is_abstract<EList>());

	class EString : public EObject {
		public:
			void hash(vcat::Hasher& hasher) const;
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

