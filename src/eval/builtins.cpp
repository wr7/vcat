#include "src/error.hh"
#include "src/eval/eobject.hh"
#include "src/eval/error.hh"
#include "src/filter/filter.hh"
#include "src/eval/builtins.hh"

#include <memory>
#include <optional>
#include <span>

namespace dvel::eval::builtins {
	// Opens a video file
	std::unique_ptr<EObject> vopen(Spanned<EList&> args) {
		const std::span<const Spanned<std::unique_ptr<EObject>>> elements = args.val.elements();

		if(elements.empty()) {
			throw eval::error::expected_file_path(args.span, {});
		}

		const EString *path_ptr = dynamic_cast<const EString *>(elements[0].val.get());

		if(!path_ptr) {
			throw eval::error::expected_file_path(elements[0].span, *elements[0].val);
		}

		if(elements.size() > 1) {
			throw eval::error::unexpected_arguments(*span_of(elements.subspan(1)));
		}

		try {
			return std::unique_ptr<EObject>(new dvel::filter::VideoFile(std::string(**path_ptr)));
		} catch(std::string err) {
			throw Diagnostic(std::move(err), {Diagnostic::Hint::error("", args.span)});
		}
	}
}

