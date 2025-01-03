#pragma once

extern "C" {
	#include <libavutil/rational.h>
	#include <libavutil/pixfmt.h>
}

namespace vcat::constants {
	constexpr AVRational TIMEBASE = {1, 90'000};
	constexpr AVPixelFormat PIXEL_FORMAT = AV_PIX_FMT_YUV420P;
}
