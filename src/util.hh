#pragma once

#include <optional>
#include <string>

namespace dvel {
	template <typename T> using OptionalRef = std::optional<std::reference_wrapper<T>>;
	std::string indent(std::string&& input, size_t level = 1);
}
