#pragma once

#include "src/filter/filter.hh"

namespace vcat::filter {
	class Concat : public VFilter {
		public:
			Concat() = delete;
			void hash(vcat::Hasher& hasher) const;
			std::string to_string() const;
			std::string type_name() const;

			std::unique_ptr<PacketSource> get_pkts(FilterContext& ctx, StreamType, Span) const;
			std::unique_ptr<FrameSource>  get_frames(FilterContext& ctx, StreamType, Span) const;

			Concat(std::vector<Spanned<const VFilter&>> videos, Span s);
		private:
			std::vector<Spanned<const VFilter&>> m_videos;
	};

	static_assert(!std::is_abstract<Concat>());
}
