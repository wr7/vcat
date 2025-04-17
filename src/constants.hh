#pragma once
#include <cstdint>
extern "C" {
	#include <libavutil/channel_layout.h>
	#include <libavutil/rational.h>
	#include <libavutil/pixfmt.h>
}

namespace vcat::constants {
	constexpr AVRational TIMEBASE = {1, 90'000};
	constexpr int64_t FALLBACK_FRAME_RATE = constants::TIMEBASE.den / (constants::TIMEBASE.num * 60); // 60Hz
	constexpr AVRational SAMPLE_ASPECT_RATIO = {1, 1};
	constexpr AVPixelFormat PIXEL_FORMAT = AV_PIX_FMT_YUV420P;
	constexpr AVColorSpace COLOR_SPACE = AVCOL_SPC_BT709;
	constexpr AVColorPrimaries COLOR_PRIMARIES = AVCOL_PRI_BT709;
	constexpr AVColorRange COLOR_RANGE = AVCOL_RANGE_MPEG;
	constexpr AVColorTransferCharacteristic COLOR_TRANSFER_FUNCTION = AVCOL_TRC_BT709;
	constexpr uint64_t CHANNEL_LAYOUT = AV_CH_LAYOUT_STEREO;
	constexpr size_t SAMPLES_PER_FRAME = 1024;
}
