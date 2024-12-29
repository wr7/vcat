#pragma once

#include <cstddef> // IWYU pragma: keep
#include <cstdint> // IWYU pragma: keep

namespace shared {
	template<typename T>
	using vcat_pointer_to = T*;

	#define vcat_def(x) x
	#define field(name, ty) ty name
	#define pointer(ty) vcat_pointer_to<ty>

	#include "raw_shared.hh"

	#undef vcat_def
	#undef field
	#undef pointer
}
