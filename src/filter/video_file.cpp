#include <cstdint>
#include <format>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <limits>

#include "src/filter/video_file.hh"
#include "libavcodec/packet.h"
#include "libavutil/avutil.h"
#include "src/constants.hh"
#include "src/filter/error.hh"
#include "src/filter/filter.hh"
#include "src/filter/util.hh"
#include "src/util.hh"

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
		, m_dts_start(0)
		, m_dts_end_info({.ts = 0, .duration = 0})
		, m_pkt_no(0)
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

		const int64_t pts = packet->pts;
		const auto bsr = binary_search_by(std::span(m_ts_info), [pts](const auto& inf){return inf.pts <=> pts;});

		assert(bsr.first);
		const PacketTimestampInfo pti = m_ts_info[bsr.second];

		packet->dts = pti.dts;
		packet->duration = pti.duration;

		assert(packet->dts != AV_NOPTS_VALUE);

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
		auto pkt_info = m_ts_info.back();

		return TsInfo {
			.ts = pkt_info.pts,
			.duration = pkt_info.duration
		};
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
		, m_file(std::move(old.m_file))
	{
		old.m_ctx = nullptr;
	}

	static void calculate_packet_duration(std::span<vcat::filter::PacketTimestampInfo> ts_info);
	static void calculate_packet_dts(std::span<vcat::filter::PacketTimestampInfo> ts_info, TsInfo& dts_end_info, int64_t& dts_start);

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

		AVPacket *pkt = av_packet_alloc();

		size_t decode_idx = 0;
		while(int res = av_read_frame(ctx, pkt) != AVERROR_EOF) {
			error::handle_ffmpeg_error(m_span,res);

			if(pkt->stream_index != m_video_idx) {
				continue;
			}

			if(pkt->pts < 0) {
				throw error::no_pts(m_span);
			}

			const int64_t old_pts = pkt->pts;

			av_packet_rescale_ts(pkt, ctx->streams[m_video_idx]->time_base, constants::TIMEBASE);

			const int64_t                 pts = pkt->pts;
			const std::pair<bool, size_t> bsr = binary_search_by(std::span(m_ts_info), [pts](const auto& t) {return t.pts <=> pts;});
			const bool                    found = bsr.first;
			const size_t                  idx = bsr.second;

			if(found) {
				throw error::duplicate_pts(m_span, old_pts);
			}

			m_ts_info.insert(m_ts_info.begin() + idx, {
				.pts = pkt->pts,
				.dts = pkt->dts,
				.duration = pkt->duration,
				.decode_idx = decode_idx,
			});

			decode_idx++;
		}

		av_packet_free(&pkt);

		calculate_packet_duration(m_ts_info);
		calculate_packet_dts(m_ts_info,m_dts_end_info, m_dts_start);

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

	static void calculate_packet_duration(std::span<vcat::filter::PacketTimestampInfo> ts_info) {
		for(size_t i = 0; i < ts_info.size(); i++) {
			if(i + 1 < ts_info.size()) {
				ts_info[i].duration = ts_info[i + 1].pts - ts_info[i].pts;
			}

			if(ts_info[i].duration <= 0 && i > 0) {
				ts_info[i].duration = ts_info[i - 1].duration;
			}

			if(ts_info[i].duration <= 0) {
				ts_info[i].duration = constants::FALLBACK_FRAME_RATE;
			}
		}
	}

	static void calculate_packet_dts(std::span<vcat::filter::PacketTimestampInfo> ts_info, TsInfo& dts_end_info, int64_t& dts_start) {
		if(ts_info.empty()) {
			return;
		}

		size_t shift_amnt = 0;

		for(size_t i = 0; i < ts_info.size(); i++) {
			if(ts_info[i].decode_idx <= i) {
				continue;
			}

			const size_t diff = ts_info[i].decode_idx - i;
			shift_amnt = diff > shift_amnt ? diff : shift_amnt;
		}

		dts_end_info.ts = std::numeric_limits<int64_t>::min();
		dts_start = std::numeric_limits<int64_t>::max();

		for(auto& info : ts_info) {
			const size_t idx = info.decode_idx;

			if(idx >= shift_amnt) {
				info.dts = ts_info[idx - shift_amnt].pts;
			} else {
				int64_t nds = static_cast<int64_t>(shift_amnt - idx);

				info.dts = (-nds) * ts_info[0].duration;
			}

			dts_start = std::min(dts_start, info.dts);

			if(info.dts > dts_end_info.ts) {
				dts_end_info.ts = info.dts;
				dts_end_info.duration = info.duration;
			}
		}
	}
}
