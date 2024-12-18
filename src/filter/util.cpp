#include <cstdlib>
#include <iostream>

extern "C" {
	#include "libavcodec/codec_par.h"
	#include "libavutil/avutil.h"
}

namespace vcat::filter::util {
	bool codecs_are_compatible(AVCodecParameters *params1, AVCodecParameters *params2) {
		if(
			params1->codec_type     != params2->codec_type     ||
			params1->codec_id       != params2->codec_id       ||
			params1->codec_tag      != params2->codec_tag      ||
			params1->format         != params2->format         ||
			params1->profile        != params2->profile        ||
			params1->level          != params2->level          ||
			params1->extradata_size != params2->extradata_size )
		{
			return false;
		}

		for(int i = 0; i < params1->extradata_size; i++) {
			if(params1->extradata[i] != params2->extradata[i]) {
				return false;
			}
		}

		if(params1->codec_type == AVMEDIA_TYPE_VIDEO) {
			return
				params1->width           == params2->width           &&
				params1->height          == params2->height          &&
				params1->field_order     == params2->field_order     &&
				params1->color_range     == params2->color_range     &&
				params1->color_primaries == params2->color_primaries &&
				params1->color_trc       == params2->color_trc       &&
				params1->color_space     == params2->color_space     &&
				params1->chroma_location == params2->chroma_location ;
		} else if(params1->codec_type == AVMEDIA_TYPE_AUDIO) {
			if(
				params1->sample_rate           != params2->sample_rate           ||
				params1->block_align           != params2->block_align           ||
				params1->frame_size            != params2->frame_size            ||
				params1->ch_layout.order       != params2->ch_layout.order       ||
				params1->ch_layout.nb_channels != params2->ch_layout.nb_channels )
			{
				return false;
			}

			switch(params1->ch_layout.order) {
				case AV_CHANNEL_ORDER_NATIVE:
				case AV_CHANNEL_ORDER_AMBISONIC:
					return params1->ch_layout.u.mask == params2->ch_layout.u.mask;
				case AV_CHANNEL_ORDER_CUSTOM:
					return false;
				case FF_CHANNEL_ORDER_NB:
				case AV_CHANNEL_ORDER_UNSPEC:
					return true;
			}
		} else {
			std::cerr << "codecs_are_compatible on non-audio/video stream";
			std::abort();
		}

		return true;
	}
}
