#pragma once

#include "src/error.hh"
#include "src/eval/eobject.hh"
#include <format>

namespace dvel::eval::error {
	using Hint = Diagnostic::Hint;

	inline Diagnostic uncallable_object(EObject& object, Span span) {
		return Diagnostic(
			std::format("Cannot call uncallable object of type `{}`", object.type_name()),
			{
				Hint::error("Opening bracket here", span)
			}
		);
	}
}

