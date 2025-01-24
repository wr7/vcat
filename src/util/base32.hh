#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace vcat {
	std::string base32_encode(std::span<const uint8_t> input);
}

