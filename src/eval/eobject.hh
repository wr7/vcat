#pragma once

#include "src/error.hh"
#include "src/util.hh"

#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace dvel {
	class EObject;
	class EList;

	using EListRef = std::reference_wrapper<EList>;

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
			virtual bool callable();
			virtual std::unique_ptr<EObject> operator()(Spanned<EListRef> args);
	};
	static_assert(std::is_abstract<EObject>());

	class VideoFile : EObject {
		public:
			VideoFile() = delete;
			void hash(dvel::Hasher& hasher) const;
			std::string to_string() const;
			std::string type_name() const;

			VideoFile(std::string&& path);

		private:
			uint8_t m_file_hash[32];
			std::string m_path;
	};
	static_assert(!std::is_abstract<VideoFile>());

	class EList : EObject {
		public:
			void hash(dvel::Hasher& hasher) const;
			std::string to_string() const;
			std::string type_name() const;

			constexpr EList(std::vector<Spanned<std::unique_ptr<EObject>>>&& elements)
				: m_elements(std::move(elements)) {}

		private:
			std::vector<Spanned<std::unique_ptr<EObject>>> m_elements;
	};
	static_assert(!std::is_abstract<EList>());

	class EString : EObject {
		public:
			void hash(dvel::Hasher& hasher) const;
			std::string to_string() const;
			std::string type_name() const;

			constexpr EString(std::string&& s)
				: m_string(std::move(s)) {}

		private:
			std::string m_string;
	};
	static_assert(!std::is_abstract<EString>());
}

