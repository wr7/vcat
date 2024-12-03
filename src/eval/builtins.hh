#pragma once

#include "src/eval/eobject.hh"

namespace vcat::eval::builtins {
	// Opens a video file
	EObject& vopen(EObjectPool& pool, Spanned<EList&> args);
	// Concatenates multiple videos
	EObject& concat(EObjectPool& pool, Spanned<EList&> args);
}

