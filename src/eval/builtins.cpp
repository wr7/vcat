#include "src/error.hh"
#include "src/eval/eobject.hh"
#include "src/eval/error.hh"
#include "src/filter/filter.hh"
#include "src/filter/concat.hh"
#include "src/filter/video_file.hh"
#include "src/eval/builtins.hh"

#include <optional>
#include <ranges>
#include <span>
#include <utility>

namespace vcat::eval::builtins {
	// Opens a video file
	const EObject& vopen(EObjectPool& pool, const Spanned<const EList&> args) {
		const std::span<const Spanned<const EObject&>> elements = args->elements();

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

	const EObject& concat(EObjectPool& pool, Spanned<const EList&> args) {
		std::vector<Spanned<const EObject&>> args_to_parse = {Spanned<const EObject&>(*args, args.span)};

		std::vector<Spanned<const filter::VFilter&>> videos;

		while(!args_to_parse.empty()) {
			Spanned<const EObject&> arg = args_to_parse.back();
			args_to_parse.pop_back();

			const EList *video_list = dynamic_cast<const EList*>(&*arg);
			if(video_list) {
				for(const auto& arg : video_list->elements() | std::ranges::views::reverse) {
					args_to_parse.push_back(arg);
				}

				continue;
			}

			const filter::VFilter *video = dynamic_cast<const filter::VFilter *>(&*arg);
			if(!video) {
				throw error::expected_video_or_list(*arg, arg.span);
			}

			videos.push_back(Spanned<const filter::VFilter&>(*video, arg.span));
		}

		return pool.add<filter::Concat>(std::move(videos), args.span);
	}

	const EObject& repeat(EObjectPool& pool, Spanned<const EList&> args) {
		if(args->elements().size() > 2) {
			throw eval::error::unexpected_arguments(*span_of(args->elements().subspan(2)));
		}

		if(args->elements().size() == 0) {
			throw eval::error::expected_argument_of_type(Span(args.span.start + args.span.length - 1, 1), "Object", {});
		}

		const EObject *reps_obj = nullptr;
		if(args->elements().size() == 2) {
			reps_obj = &*args->elements()[1];
		}

		const EInteger *num_reps = dynamic_cast<const EInteger *>(reps_obj);
		if(!num_reps) {
			std::optional<std::reference_wrapper<const EObject>> obj;
			if(reps_obj) {
				obj = *reps_obj;
			}

			throw eval::error::expected_argument_of_type(
				reps_obj ? args->elements()[1].span : Span(args.span.start + args.span.length - 1, 1),
				"Integer",
				obj
			);
		}

		if(**num_reps < 0) {
			throw eval::error::expected_positive_number(args->elements()[1].span, **num_reps);
		}

		std::vector<Spanned<const EObject&>> elements;

		for(int64_t i = 0; i < **num_reps; i++) {
			elements.push_back(args->elements()[0]);
		}

		return pool.add<EList>(std::move(elements));
	}
}

