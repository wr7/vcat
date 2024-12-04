#pragma once

extern "C" {
	#include "libavutil/rational.h"
}

namespace vcat::constants {
	extern inline const AVRational TIMEBASE = {1, 90'000};
}
