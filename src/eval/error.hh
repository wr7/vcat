#pragma once

#include "src/error.hh"
#include "src/eval/eobject.hh"
#include <format>
#include <functional>
#include <optional>

namespace vcat::eval::error {
	using Hint = Diagnostic::Hint;

	inline Diagnostic uncallable_object(const EObject& object, Span span) {
		return Diagnostic(
			std::format("Cannot call uncallable object of type `{}`", object.type_name()),
			{
				Hint::error("", span)
			}
		);
	}

	inline Diagnostic expected_file_path(Span s, std::optional<std::reference_wrapper<const EObject>> other) {
		if(!other.has_value()) {
			return Diagnostic(
				"Expected file path",
				{
					Hint::error("", s)
				}
			);
		}
		return Diagnostic(
			std::format("Expected file path (String); got `{}`", other->get().type_name()),
			{
				Hint::error("", s)
			}
		);
	}

	inline Diagnostic unexpected_arguments(Span s) {
		return Diagnostic(
			"Unexpected argument(s)",
			{
				Hint::error("", s)
			}
		);
	}

	inline Diagnostic undefined_variable(Spanned<std::string_view> s) {
		return Diagnostic(
			std::format("Undefined variable {}", s.val),
			{
				Hint::error("", s.span)
			}
		);
	}

	inline Diagnostic expected_video_or_list(const EObject& object, Span span) {
		return Diagnostic(
			std::format("Expected video or list of videos; got object of type `{}`", object.type_name()),
			{
				Hint::error("", span)
			}
		);
	}

	inline Diagnostic unexpected_character_in_number(char c, Span span) {
		return Diagnostic(
			std::format("Unexpected character `{}` in number", c),
			{
				Hint::error("", span)
			}
		);
	}

	inline Diagnostic integer_too_large(std::string_view s, Span span) {
		return Diagnostic(
			std::format("The integer `{}` is too large", s),
			{
				Hint::error("", span)
			}
		);
	}
}

