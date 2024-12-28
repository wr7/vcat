#pragma once

extern "C" {
	#include "libavutil/rational.h"
}

namespace vcat::constants {
	extern inline constexpr AVRational TIMEBASE = {1, 90'000};
}
