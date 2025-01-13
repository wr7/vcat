#include <sstream>

#include "src/filter/concat.hh"
#include "src/filter/error.hh"
#include "src/filter/util.hh"

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

	class ConcatFrameSource : public FrameSource {
		public:
			ConcatFrameSource() = delete;
			ConcatFrameSource(std::span<const Spanned<const VFilter&>> videos, const VideoParameters& params)
				: m_idx(0)
				, m_last_pts(0)
				, m_last_dur(0)
				, m_pts_offset(0)
			{
				for(const auto& video : videos) {
					m_videos.push_back(video->get_frames(video.span, params));
				}
			}

			bool next_frame(AVFrame **p_frame) {
				if(m_idx >= m_videos.size()) {
					return false;
				}

				AVFrame *frame = *p_frame;

				if(!m_videos[m_idx]->next_frame(p_frame)) {
					m_idx += 1;
					m_pts_offset = m_last_pts + m_last_dur;
					return this->next_frame(p_frame);
				}

				frame->pts += m_pts_offset;

				if(frame->duration != 0) {
					m_last_dur = frame->duration;
				} else {
					assert(frame->pts >= m_last_pts);
					m_last_dur = frame->pts - m_last_pts;
				}

				m_last_pts = frame->pts;

				return true;
			}

		private:
			size_t  m_idx;
			int64_t m_last_pts;
			int64_t m_last_dur;
			int64_t m_pts_offset;
			std::vector<std::unique_ptr<FrameSource>> m_videos;
	};

	std::unique_ptr<FrameSource> Concat::get_frames(Span, const VideoParameters& params) const {
		return std::make_unique<ConcatFrameSource>(m_videos, params);
	}

	class ConcatPktSource : public PacketSource {
		public:
			ConcatPktSource() = delete;

			ConcatPktSource(std::span<const Spanned<const VFilter&>> videos, Span, const VideoParameters& params)
				: m_idx(0)
			{

				const AVCodecParameters *prev_param = nullptr;

				for(const auto& video : videos) {
					m_videos.push_back(video->get_pkts(video.span, params));
					const AVCodecParameters *cur_param = m_videos.back()->video_codec();

					if(prev_param) {
						assert(util::codecs_are_compatible(cur_param, prev_param)); // TODO: generate an actual error message
					}

					prev_param = cur_param;
				}

				calculate_ts_offsets();
			}

			bool next_pkt(AVPacket **p_packet) {
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
				packet->dts += m_dts_offsets[m_idx];

				return true;
			}

			const AVCodecParameters *video_codec() {
				return m_videos[0]->video_codec();
			}

			TsInfo dts_end_info() const {
				TsInfo ts_info = m_videos.back()->dts_end_info();

				ts_info.ts += m_dts_offsets.back();
				return ts_info;
			}

			int64_t dts_start() const {
				return m_videos.front()->dts_start() + m_videos.front()->dts_start();
			}

			TsInfo pts_end_info() const {
				TsInfo ts_info = m_videos.back()->pts_end_info();

				ts_info.ts += m_pts_offsets.back();
				return ts_info;
			}

		private:
			void calculate_ts_offsets() {
				assert(!m_videos.empty());

				m_dts_offsets.reserve(m_videos.size());
				m_pts_offsets.reserve(m_videos.size());

				{ // Calculate pts offsets //
					size_t pts_offset = 0;

					for(auto& vid : m_videos) {
						m_pts_offsets.push_back(pts_offset);

						TsInfo pts_info = vid->pts_end_info();
						pts_offset += pts_info.ts + pts_info.duration;
					}
				}

				{ // Calculate dts offsets //
					m_dts_offsets.push_back(m_pts_offsets.back());;

					if(m_videos.size() == 1) {
						return;
					}

					int64_t end = m_pts_offsets.back() + m_videos.back()->dts_start();

					for(size_t i = m_videos.size() - 1; i-- > 0;) {
						const auto& video = m_videos[i];

						const TsInfo  old_end   = video->dts_end_info();
						const int64_t old_start = video->dts_start();

						const int64_t new_pts_start = m_pts_offsets[i];
						const int64_t new_end   = end - old_end.duration;
						const int64_t new_start = new_end - (old_end.ts - old_start);

						const int64_t max_new_start = new_pts_start + old_start;

						if(new_start > max_new_start) {
							m_dts_offsets.push_back(max_new_start - old_start);
							end = max_new_start;
							continue;
						}

						m_dts_offsets.push_back(new_start - old_start);
						end = new_start;
					}

					std::reverse(m_dts_offsets.begin(), m_dts_offsets.end());
				}
			}

			std::vector<std::unique_ptr<PacketSource>> m_videos;
			std::vector<int64_t>                       m_dts_offsets; //< How much the dts of every packet in the videos should be offset
			std::vector<int64_t>                       m_pts_offsets; //< How much the pts of every packet in the videos should be offset
			size_t                                     m_idx;
	};

	std::unique_ptr<PacketSource> Concat::get_pkts(Span s, const VideoParameters& params) const {
		return std::make_unique<ConcatPktSource>(m_videos, s, params);
	}
}
