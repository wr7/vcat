#pragma once

#include "src/error.hh"
#include <format>
#include <string_view>

extern "C" {
	#include "libavutil/error.h"
}

namespace dvel::filter::error {
	using Hint = Diagnostic::Hint;

	inline Diagnostic ffmpeg_error(std::string_view msg, Span s) {
		return Diagnostic(
			std::format("FFMPEG error: {}", msg),
			{
				Hint::error("", s)
			}
		);
	}

	inline Diagnostic expected_video_got_none(Span s) {
		return Diagnostic(
			"Expected video parameter; got none",
			{
				Hint::error("", s)
			}
		);
	}

	inline void handle_ffmpeg_error(Span s, int err_code) {
		if(err_code < 0) {
			char msg[AV_ERROR_MAX_STRING_SIZE] = {0};

			av_strerror(err_code, msg, sizeof(msg));

			throw ffmpeg_error(msg, s);
		}
	}
}
