#pragma once

#include "src/eval/eobject.hh"
#include "src/shared.hh"

namespace vcat::muxing {
	void write_output(Spanned<const vcat::EObject&> eobject, const shared::Parameters& params);
}

