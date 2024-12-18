#pragma once

extern "C" {
	#include "libavcodec/codec_par.h"
}

namespace vcat::filter::util {
	bool codecs_are_compatible(AVCodecParameters *params1, AVCodecParameters *params2);
}
