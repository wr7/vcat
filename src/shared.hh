#pragma once

#include <cstddef> // IWYU pragma: keep
#include <cstdint> // IWYU pragma: keep
#include <cstdlib>

namespace shared {
	template<typename T>
	using vcat_pointer_to = T*;

	#define vcat_def(x) x
	#define field(name, ty) union{ty name;};
	#define pointer(ty) vcat_pointer_to<ty>

	#define has_destructor() inline ~STRUCT_NAME()

	#define rust_drop(name) \
		extern "C" void name(STRUCT_NAME); \
		inline STRUCT_NAME::~STRUCT_NAME() {name(*this);}

	#include "raw_shared.hh"

	#undef vcat_def
	#undef field
	#undef pointer

	template<typename T>
	inline Vector<T>::~Vector() {
		free(data);

		data = nullptr;
		capacity = 0;
		length = 0;
	}
}
