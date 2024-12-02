#include "src/error.hh"
#include "src/eval/eobject.hh"
#include "src/eval/error.hh"
#include "src/filter/filter.hh"
#include "src/eval/builtins.hh"

#include <memory>
#include <optional>
#include <span>
#include <utility>

namespace vcat::eval::builtins {
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
			return std::make_unique<vcat::filter::VideoFile>(std::string(**path_ptr));
		} catch(std::string err) {
			throw Diagnostic(std::move(err), {Diagnostic::Hint::error("", args.span)});
		}
	}

	std::unique_ptr<EObject> concat(Spanned<EList&> args) {
		std::vector<Spanned<std::unique_ptr<filter::VFilter>>> videos;

		for(const auto& arg : args->elements()) {
			EObject *arg_ptr = arg->get();

			filter::VFilter *video = dynamic_cast<filter::VFilter *>(arg_ptr);
			if(!video) {
				throw error::expected_video(**arg, arg.span);
			}

			videos.push_back(Spanned(video->clone(), arg.span));
		}

		return std::make_unique<filter::Concat>(std::move(videos), args.span);
	}
}

