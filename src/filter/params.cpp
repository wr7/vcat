#include "src/filter/params.hh"
#include <cstdint>

namespace vcat::filter {
	void VideoParameters::hash(Hasher& hasher) const {
		hasher.add("_video-parameters_");
		const size_t start = hasher.pos();

		hasher.add(width);
		hasher.add(height);
		hasher.add(fixed_fps);
		hasher.add((int64_t) fps);

		hasher.add(static_cast<uint64_t>(hasher.pos() - start));
	}

	void AudioParameters::hash(Hasher& hasher) const {
		hasher.add("_audio-parameters_");
		const size_t start = hasher.pos();

		hasher.add(sample_rate);
		hasher.add(static_cast<uint8_t>(sample_format));
		hasher.add(channel_layout);

		hasher.add(static_cast<uint64_t>(hasher.pos() - start));
	}
}
