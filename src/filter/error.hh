#pragma once

#include "src/error.hh"
#include <cstdint>
#include <cstring>
#include <format>
#include <string_view>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavutil/error.h>
}

namespace vcat::filter::error {
	using Hint = Diagnostic::Hint;

	inline Diagnostic ffmpeg_error(std::string_view msg, Span s, int error_code) {
		return Diagnostic(
			std::format("FFMPEG error: {} ({})", msg, error_code),
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

	inline Diagnostic duplicate_pts(Span s, int64_t pts) {
		return Diagnostic(
			std::format("Multiple packets found with pts `{}`", pts),
			{
				Hint::error("", s)
			}
		);
	}

	inline Diagnostic no_pts(Span s) {
		return Diagnostic(
			"No pts value found for frame",
			{
				Hint::error("", s)
			}
		);
	}

	inline Diagnostic no_dts(Span s) {
		return Diagnostic(
			"No dts value found for frame",
			{
				Hint::error("", s)
			}
		);
	}

	inline Diagnostic failed_cache_directory(Span s, std::string_view msg) {
		return Diagnostic(
			std::format("Failed to create cache directory: {}", msg),
			{
				Hint::error("", s)
			}
		);
	}

	inline Diagnostic failed_file_open(Span s, std::string_view filename) {
		return Diagnostic(
			std::format("Failed to open file `{}`: {}", filename, strerror(errno)),
			{
				Hint::error("", s)
			}
		);
	}

	inline Diagnostic no_video(Span s, std::string_view filename) {
		return Diagnostic(
			std::format("No video stream found in file `{}`", filename),
			{
				Hint::error("", s)
			}
		);
	}

	inline Diagnostic unsupported_output_codec_params(Span s, std::string_view msg) {
		return Diagnostic(
			std::format("Unsupported output codec parameters: {}", msg),
			{
				Hint::error("", s)
			}
		);
	}

	inline Diagnostic unimplemented_channel_order(Span s) {
		return Diagnostic(
			"Channel order has not been implemented yet",
			{
				Hint::error("", s)
			}
		);
	}

	inline Diagnostic ffmpeg_no_codec(Span s, AVCodecID codec_id) {
		return Diagnostic(
			std::format("The local build of FFMPEG does not support codec `{}`", avcodec_get_name(codec_id)),
			{
				Hint::error("", s)
			}
		);
	}

	inline Diagnostic ffmpeg_no_filter(Span s, std::string_view filter) {
		return Diagnostic(
			std::format("The local build of FFMPEG does not support filter `{}`", filter),
			{
				Hint::error("", s)
			}
		);
	}

	inline void handle_ffmpeg_error(Span s, int err_code) {
		if(err_code < 0) {
			char msg[AV_ERROR_MAX_STRING_SIZE] = {0};

			av_strerror(err_code, msg, sizeof(msg));

			throw ffmpeg_error(msg, s, err_code);
		}
	}
}
