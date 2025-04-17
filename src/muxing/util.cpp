#include "src/muxing/util.hh"
#include "src/muxing/error.hh"

namespace vcat::muxing {
	PacketInterleaver::PacketInterleaver(std::unique_ptr<filter::PacketSource>&& video, std::unique_ptr<filter::PacketSource>&& audio)
		: m_video(std::move(video))
		, m_audio(std::move(audio))
		, m_video_eof(false)
		, m_video_buf(av_packet_alloc())
		, m_audio_eof(false)
		, m_audio_buf(av_packet_alloc())
	{
		error::handle_ffmpeg_error(m_video_buf && m_audio_buf ? 0 : AVERROR(ENOMEM));
	}

	bool PacketInterleaver::next(AVPacket **p_output) {
		if(!*p_output) {
			*p_output = av_packet_alloc();
			error::handle_ffmpeg_error(p_output ? 0 : AVERROR(ENOMEM));
		}

		av_packet_unref(*p_output);

		if(!m_video_buf->data && !m_video_eof) {
			m_video_eof = !m_video->next_pkt(&m_video_buf);
			if(!m_video_eof) {
				m_video_buf->stream_index = 0;
			}
		}

		if(!m_audio_buf->data && !m_audio_eof) {
			m_audio_eof = !m_audio->next_pkt(&m_audio_buf);
			if(!m_audio_eof) {
				m_audio_buf->stream_index = 1;
			}
		}

		AVPacket **selected_packet;

		if(m_video_buf->data && m_audio_buf->data) {
			selected_packet = m_video_buf->dts <= m_audio_buf->dts
				? &m_video_buf
				: &m_audio_buf;
		} else if(m_video_buf->data) {
			selected_packet = &m_video_buf;
		} else if(m_audio_buf->data) {
			selected_packet = &m_audio_buf;
		} else {
			return false;
		}

		std::swap(*p_output, *selected_packet);
		return true;
	}

	PacketInterleaver::~PacketInterleaver() {
		av_packet_free(&m_video_buf);
		av_packet_free(&m_audio_buf);
	}
}

