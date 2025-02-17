#pragma once

extern "C" {
	#include <libavutil/rational.h>
	#include <libavutil/pixfmt.h>
}

namespace vcat::constants {
	constexpr AVRational TIMEBASE = {1, 90'000};
	constexpr int64_t FALLBACK_FRAME_RATE = constants::TIMEBASE.den / (constants::TIMEBASE.num * 60); // 60Hz
	constexpr AVRational SAMPLE_ASPECT_RATIO = {1, 1};
	constexpr AVPixelFormat PIXEL_FORMAT = AV_PIX_FMT_YUV420P;
}
