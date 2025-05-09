#include <format>
#include <iostream>
#include <sstream>

#include "src/filter/concat.hh"
#include "src/filter/error.hh"
#include "src/filter/filter.hh"
#include "src/filter/util.hh"
#include "src/util.hh"

namespace vcat::filter {
	Concat::Concat(std::vector<Spanned<const VFilter&>> videos, Span s) : m_videos(std::move(videos)) {
		if(m_videos.empty()) {
			throw error::expected_video_got_none(s);
		}
	}

	void Concat::hash(Hasher& hasher) const {
		hasher.add("_concat-filter_");
		const size_t start = hasher.pos();

		for(const auto& video : m_videos) {
			video->hash(hasher);
		}

		hasher.add((uint64_t) (hasher.pos() - start));
	}

	std::string Concat::to_string() const {
		std::stringstream s;

		std::string_view separator = m_videos.size() > 1 ? ",\n" : "";

		s << "Concat(";

		if(m_videos.size() > 1) {
			s << '\n';
		}

		for(const auto& video : m_videos) {
			s
				<< indent(video->to_string())
				<< separator;
		}

		s << ")";

		return s.str();
	}

	std::string Concat::type_name() const {
		return "Concat";
	}

	std::unique_ptr<FrameSource> Concat::get_frames(FilterContext& fctx, StreamType type, Span span) const {
		std::vector<std::unique_ptr<FrameSource>> videos;

		for(const auto& video : m_videos) {
			videos.push_back(video.val.get_frames(fctx, type, video.span));
		}

		if(videos.size() == 1) {
			return std::move(videos[0]);
		}

		std::string filter;

		for(size_t i = 0; i < videos.size(); i++) {
			filter += std::format("[in_{}] ", i);
		}

		filter += std::format("concat=n={}", videos.size());

		if(type == StreamType::Audio) {
			filter += ":v=0:a=1";
		}

		filter += " [out]";

		std::vector<StreamType> input_types{videos.size(), type};

		return std::make_unique<FFMpegFilter>(span, std::move(videos), filter.c_str(), fctx, input_types, type);
	}

	class ConcatPktSource : public PacketSource {
		public:
			ConcatPktSource() = delete;

			ConcatPktSource(std::span<const Spanned<const VFilter&>> videos, FilterContext& ctx,StreamType type, Span)
				: m_pkt_idx(static_cast<size_t>(0) - 1)
				, m_idx(0)
			{
				assert(type == StreamType::Video);

				const AVCodecParameters *prev_param = nullptr;

				for(const auto& video : videos) {
					m_videos.push_back(video->get_pkts(ctx, type, video.span));
					const AVCodecParameters *cur_param = m_videos.back()->video_codec();

					if(prev_param) {
						assert(util::codecs_are_compatible(cur_param, prev_param));
					}

					prev_param = cur_param;
				}

				calculate_ts_offsets();
			}

			bool next_pkt(AVPacket **p_packet) {
				m_pkt_idx += 1;

				for(;;) {
					if(m_idx >= m_videos.size()) {
						return false;
					}

					if(m_videos[m_idx]->next_pkt(p_packet)) {
						break;
					} else {
						m_idx++;
					}
				}

				AVPacket *packet = *p_packet;

				packet->pts += m_pts_offsets[m_idx];

				size_t idx = binary_search(std::span(m_prev_pts), packet->pts).second;
				m_prev_pts.insert(m_prev_pts.begin() + idx, packet->pts);

				if(m_pkt_idx >= m_dts_shift) {
					packet->dts = m_prev_pts[m_pkt_idx - m_dts_shift];
					return true;
				}

				packet->dts = -first_pkt_duration() * static_cast<int64_t>(m_dts_shift - m_pkt_idx);

				return true;
			}

			const AVCodecParameters *video_codec() {
				return m_videos[0]->video_codec();
			}

			int64_t first_pkt_duration() const {
				return m_videos[0]->first_pkt_duration();
			}

			size_t dts_shift() const {
				return m_dts_shift;
			}

			TsInfo pts_end_info() const {
				TsInfo ts_info = m_videos.back()->pts_end_info();

				ts_info.ts += m_pts_offsets.back();
				return ts_info;
			}

		private:
			void calculate_ts_offsets() {
				assert(!m_videos.empty());

				m_dts_shift = 0;
				m_pts_offsets.reserve(m_videos.size());

				size_t pts_offset = 0;

				for(auto& vid : m_videos) {
					m_dts_shift = std::max(m_dts_shift, vid->dts_shift());

					m_pts_offsets.push_back(pts_offset);

					TsInfo pts_info = vid->pts_end_info();
					pts_offset += pts_info.ts + pts_info.duration;
				}
			}

			std::vector<std::unique_ptr<PacketSource>> m_videos;
			size_t                                     m_dts_shift;
			std::vector<int64_t>                       m_pts_offsets; //< How much the pts of every packet in the videos should be offset
			std::vector<int64_t>                       m_prev_pts;
			size_t                                     m_pkt_idx;
			size_t                                     m_idx;
	};

	std::unique_ptr<PacketSource> Concat::get_pkts(FilterContext& ctx, StreamType type, Span s) const {
		return std::make_unique<ConcatPktSource>(m_videos, ctx, type, s);
	}
}
