#pragma once

#include "src/util.hh"

namespace vcat::filter {
	class VideoParameters {
		public:
			void hash(Hasher& hasher) const;

			int width;  //< Width (in pixels)
			int height; //< Height (in pixels)

			bool fixed_fps;

			double fps;
	};
}
