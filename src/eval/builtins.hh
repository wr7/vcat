#pragma once

#include "src/eval/eobject.hh"

namespace vcat::eval::builtins {
	// Opens a video file
	const EObject& vopen(EObjectPool& pool, Spanned<const EList&> args);
	// Concatenates multiple videos
	const EObject& concat(EObjectPool& pool, Spanned<const EList&> args);
}

