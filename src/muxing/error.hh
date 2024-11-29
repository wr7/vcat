#pragma once

#include "src/error.hh"
#include <cstdlib>
#include <iostream>

extern "C" {
	#include "libavutil/error.h"
}

namespace dvel::muxing::error {
	using Hint = Diagnostic::Hint;

	inline Diagnostic invalid_output(Span span) {
		return Diagnostic(
			"Invalid script output; output must be a video object",
			{
				Hint::error("", span)
			}
		);
	}

	inline void handle_ffmpeg_error(int err_code) {
		if(err_code < 0) {
			char msg[AV_ERROR_MAX_STRING_SIZE] = {0};

			av_strerror(err_code, msg, sizeof(msg));

			std::cerr << "Failed to write to output file `output.mkv`: " << &*msg;

			std::abort();
		}
	}
}
