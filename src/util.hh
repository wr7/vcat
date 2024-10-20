#pragma once

#include <optional>

namespace dvel {
	template <typename T> using OptionalRef = std::optional<std::reference_wrapper<T>>;
}
