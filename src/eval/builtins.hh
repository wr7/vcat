#pragma once

#include "src/eval/eobject.hh"

namespace dvel::eval::builtins {
	// Opens a video file
	std::unique_ptr<EObject> vopen(Spanned<EList&> args);
	// Concatenates multiple videos
	std::unique_ptr<EObject> concat(Spanned<EList&> args);
}

