#pragma once

#include <cstddef> // IWYU pragma: keep
#include <cstdint> // IWYU pragma: keep
#include <cstdlib>

namespace shared {
	template<typename T>
	using vcat_pointer_to = T*;

	#define field(name, ty) union{ty name;}
	#define pointer(ty) vcat_pointer_to<ty>

	#define has_destructor() inline ~STRUCT_NAME()

	#define rust_drop(name) \
		extern "C" void name(STRUCT_NAME *); \
		inline STRUCT_NAME::~STRUCT_NAME() {name(this);}

	#include "raw_shared.hh"

	#undef STRUCT_NAME
	#undef has_destructor
	#undef rust_drop

	#undef field
	#undef pointer

	extern "C" Parameters vcat_cli_parse();
	extern "C" void rustalloc_free(void *data, size_t size, size_t align);

	template<typename T>
	inline Vector<T>::~Vector() {
		rustalloc_free(data, sizeof(T) * capacity, alignof(T));

		data = nullptr;
		capacity = 0;
		length = 0;
	}
}
