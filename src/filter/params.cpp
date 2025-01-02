#include "src/filter/params.hh"
#include <cstdint>

namespace vcat::filter {
	void VideoParameters::hash(Hasher& hasher) const {
		hasher.add("_video-parameters_");
		const size_t start = hasher.pos();

		hasher.add(width);
		hasher.add(height);

		hasher.add(static_cast<uint64_t>(hasher.pos() - start));
	}
}
