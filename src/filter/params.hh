#pragma once

#include "src/shared.hh"
#include "src/util.hh"
#include <cstdint>

namespace vcat::filter {
	class VideoParameters {
		public:
			void hash(Hasher& hasher) const;

			int width;  //< Width (in pixels)
			int height; //< Height (in pixels)

			bool fixed_fps;

			double fps;
	};

	class AudioParameters {
		public:
			void hash(Hasher& hasher) const;

			int sample_rate; //< Sample rate (in Hz)
			shared::SampleFormat sample_format;
			uint64_t channel_layout;
	};
}
