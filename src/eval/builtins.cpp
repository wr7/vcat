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
	EObject& vopen(EObjectPool& pool, Spanned<EList&> args) {
		const std::span<const Spanned<EObject&>> elements = args.val.elements();

		if(elements.empty()) {
			throw eval::error::expected_file_path(args.span, {});
		}

		const EString *path_ptr = dynamic_cast<const EString *>(&*elements[0]);

		if(!path_ptr) {
			throw eval::error::expected_file_path(elements[0].span, *elements[0]);
		}

		if(elements.size() > 1) {
			throw eval::error::unexpected_arguments(*span_of(elements.subspan(1)));
		}

		try {
			return pool.add<vcat::filter::VideoFile>(std::string(**path_ptr));
		} catch(std::string err) {
			throw Diagnostic(std::move(err), {Diagnostic::Hint::error("", args.span)});
		}
	}

	EObject& concat(EObjectPool& pool, Spanned<EList&> args) {
		std::vector<Spanned<std::unique_ptr<filter::VFilter>>> videos;

		for(const auto& arg : args->elements()) {
			EObject *arg_ptr = &*arg;

			filter::VFilter *video = dynamic_cast<filter::VFilter *>(arg_ptr);
			if(!video) {
				throw error::expected_video(*arg, arg.span);
			}

			videos.push_back(Spanned(video->clone(), arg.span));
		}

		return pool.add<filter::Concat>(std::move(videos), args.span);
	}
}

