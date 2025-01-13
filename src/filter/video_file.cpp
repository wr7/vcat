#include <format>
#include <iomanip>
#include <fstream>

#include "src/filter/video_file.hh"
#include "src/constants.hh"
#include "src/filter/error.hh"
#include "src/filter/util.hh"

namespace vcat::filter {
	void VideoFile::hash(Hasher& hasher) const {
		hasher.add("_videofile_");
		hasher.add(&m_file_hash, sizeof(m_file_hash));
		hasher.add((uint64_t) sizeof(m_file_hash));
	}

	// NOTE: throws `std::string` upon IO failure
	VideoFile::VideoFile(std::string&& path) : m_path(std::move(path)) {
		std::ifstream f;
		f.open(m_path, std::ios_base::in | std::ios_base::binary);

		char buf[512];

		f.rdbuf()->pubsetbuf(0, 0);

		Hasher hasher;

		while(f.good()) {
			size_t num_bytes = f.read(&buf[0], sizeof(buf)).gcount();

			hasher.add(&buf[0], num_bytes);
		}

		std::array<uint8_t, 32> hash = hasher.into_bin();
		std::copy_n(hash.begin(), hash.size(), m_file_hash);

		if(f.bad()) {
			throw std::format("Failed to open file `{}`: {}", m_path, strerror(errno));
		}

		f.close();
	}

	std::string VideoFile::to_string() const {
		std::stringstream s;
		s << "VideoFile(" << std::quoted(m_path) << ")";

		return s.str();
	}

	std::string VideoFile::type_name() const {
		return "VideoFile";
	}

	VideoFilePktSource::VideoFilePktSource(const std::string& path, Span span)
		: m_ctx(avformat_alloc_context())
		, m_span(span)
		, m_pkt_no(0)
		, m_dts_jump(0)
		, m_file()
	{
		errno = 0;
		FILE *fp = fopen(path.c_str(), "rb");  

		if(errno != 0) {
			throw error::failed_file_open(m_span, path);
		}

		new(&m_file) util::VCatAVFile(fp);

		calculate_info(path);

		m_ctx->flags |= AVFMT_FLAG_SORT_DTS | AVFMT_FLAG_GENPTS | AVFMT_AVOID_NEG_TS_MAKE_ZERO;
		m_ctx->pb = m_file.get();

		error::handle_ffmpeg_error(m_span,
			avformat_open_input(&m_ctx, path.c_str(), NULL, NULL)
		);

		error::handle_ffmpeg_error(m_span,
			avformat_find_stream_info(m_ctx, NULL)
		);
	}

	bool VideoFilePktSource::next_pkt(AVPacket **p_packet) {
		AVPacket *const packet = *p_packet;

		int res = av_read_frame(m_ctx, *p_packet);
		if(res == AVERROR_EOF) {
			return false;
		}

		error::handle_ffmpeg_error(m_span, res);

		packet->stream_index = 0;

		const AVRational old_timebase = m_ctx->streams[packet->stream_index]->time_base;

		av_packet_rescale_ts(packet, old_timebase, constants::TIMEBASE);

		if(packet->dts == AV_NOPTS_VALUE) {
			packet->dts = m_dts_start + m_pkt_no * m_dts_jump;
		}

		m_pkt_no += 1;
		return true;
	}

	const AVCodecParameters *VideoFilePktSource::video_codec() {
		return m_ctx->streams[m_video_idx]->codecpar;
	}

	std::span<AVStream *> VideoFilePktSource::av_streams() {
		return std::span<AVStream *>(m_ctx->streams, m_ctx->nb_streams);
	}

	TsInfo VideoFilePktSource::dts_end_info() const {
		return m_dts_end_info;
	}

	int64_t VideoFilePktSource::dts_start() const {
		return m_dts_start;
	}

	TsInfo VideoFilePktSource::pts_end_info() const {
		return m_pts_end_info;
	}

	VideoFilePktSource::~VideoFilePktSource() {
		if(m_ctx != nullptr) {
			avformat_close_input(&m_ctx);
		}
	}

	VideoFilePktSource::VideoFilePktSource(VideoFilePktSource&& old)
		: m_ctx(old.m_ctx)
		, m_span(old.m_span)
		, m_pkt_no(old.m_pkt_no)
		, m_dts_jump(old.m_dts_jump)
	{
		old.m_ctx = nullptr;
	}

	// Walks through the file to calculate `m_dts_start`, `m_dts_end_info`, `m_pts_end_info`, and `video_idx`
	//
	// NOTE: this should be called before the main AVFormatContext is created.
	void VideoFilePktSource::calculate_info(const std::string& path) {
		AVFormatContext *ctx = avformat_alloc_context();
		ctx->pb = m_file.get();
		ctx->flags |= AVFMT_FLAG_SORT_DTS | AVFMT_FLAG_GENPTS | AVFMT_AVOID_NEG_TS_MAKE_ZERO;

		error::handle_ffmpeg_error(m_span,
			avformat_open_input(&ctx, path.c_str(), NULL, NULL)
		);

		error::handle_ffmpeg_error(m_span,
			avformat_find_stream_info(ctx, NULL)
		);

		bool has_video_idx = false;
		for(size_t i = 0; i < ctx->nb_streams; i++) {
			if(ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
				m_video_idx = i;
				has_video_idx = true;
				break;
			}
		}

		if(!has_video_idx) {
			throw error::no_video(m_span, path);
		}

		std::optional<int64_t> first_defined_dts = {};
		int64_t num_undef_dts = 0;

		m_dts_end_info = {0, 0};
		m_pts_end_info = {0, 0};
		m_dts_start = 0;

		AVPacket *pkt = av_packet_alloc();

		while(int res = av_read_frame(ctx, pkt) != AVERROR_EOF) {
			error::handle_ffmpeg_error(m_span, res);

			if(pkt->stream_index == m_video_idx) {
				av_packet_rescale_ts(pkt, ctx->streams[pkt->stream_index]->time_base, constants::TIMEBASE);

				// Some formats (ie mkv) do not support negative DTS and instead use `AV_NOPTS_VALUE`
				//
				// Some of vcat's filters do not like this, so we need to manually reconstruct the DTS. That's
				// what this information is for.
				if(!first_defined_dts) {
					if(pkt->dts == AV_NOPTS_VALUE) {
						num_undef_dts++;
						continue;
					} else {
						// It would be genuinely insane if a file with this property did not start with a DTS of
						// zero, but we will handle this case anyway
						first_defined_dts = pkt->dts;

						if(pkt->duration > 0) {
							m_dts_jump = pkt->duration;
						} else {
							m_dts_jump = constants::FALLBACK_FRAME_RATE; // If worst comes to worst, we will just fall back to 60Hz
						}
					}
				}

				if(pkt->dts == AV_NOPTS_VALUE) {
					throw error::no_dts(m_span);
				}

				if(pkt->pts == AV_NOPTS_VALUE) {
					throw error::no_pts(m_span);
				}

				if(pkt->dts >= m_dts_end_info.ts) {
					TsInfo& packet_info = m_dts_end_info;

					packet_info.ts = pkt->dts;
					packet_info.duration = pkt->duration;
				}

				if(pkt->dts < m_dts_start) {
					m_dts_start = pkt->dts;
				}

				if(pkt->pts >= m_pts_end_info.ts) {
					TsInfo& packet_info = m_pts_end_info;

					packet_info.ts = pkt->pts;
					packet_info.duration = pkt->duration;
				}
			}

			av_packet_unref(pkt);
		}

		if(num_undef_dts > 0) {
			if(!first_defined_dts) {
				throw error::no_dts(m_span);
			}

			m_dts_start = *first_defined_dts - num_undef_dts * m_dts_jump;
		}

		av_packet_free(&pkt);
		avformat_close_input(&ctx);

		m_file.reset();
	}

	std::unique_ptr<FrameSource> VideoFile::get_frames(Span span, const VideoParameters& params) const {
		return std::make_unique<Decode>(
			span,
			std::make_unique<VideoFilePktSource>(m_path, span),
			params
		);
	}
}
