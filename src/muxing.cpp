#include "src/eval/eobject.hh"
#include "src/muxing/error.hh"
#include "src/filter/filter.hh"
#include "src/muxing.hh"

extern "C" {
	#include "libavcodec/codec_par.h"
	#include "libavcodec/packet.h"
	#include "libavformat/avformat.h"
	#include "libavutil/error.h"
	#include "libavutil/mathematics.h"
}

namespace vcat::muxing {
	void write_output(Spanned<const vcat::EObject&> eobject) {
		vcat::Span span = eobject.span;

		const vcat::filter::VFilter *filter = dynamic_cast<const vcat::filter::VFilter *>(&eobject.val);

		if(!filter) {
			throw error::invalid_output(span);
		}

		std::unique_ptr<filter::PacketSource> source = filter->get_pkts(span);
		std::span<AVStream *> streams = source->streams();

		AVFormatContext *output = nullptr;
		error::handle_ffmpeg_error(
			avformat_alloc_output_context2(&output, nullptr, nullptr, "output.mp4")
		);

		for(AVStream *istream : streams) {
			AVStream *ostream = avformat_new_stream(output, nullptr);

			error::handle_ffmpeg_error(
				ostream ? 0 : AVERROR_UNKNOWN
			);

			error::handle_ffmpeg_error(
				avcodec_parameters_copy(ostream->codecpar, istream->codecpar)
			);
		}

		if(!(output->flags & AVFMT_NOFILE)) {
			error::handle_ffmpeg_error(
				avio_open(&output->pb, "output.mp4", AVIO_FLAG_WRITE)
			);
		}

		error::handle_ffmpeg_error(
			avformat_write_header(output, nullptr)
		);

		AVPacket *packet = av_packet_alloc();
		error::handle_ffmpeg_error(
			packet ? 0 : AVERROR_UNKNOWN
		);

		while(source->next_pkt(&packet)) {
			AVStream *istream = streams[packet->stream_index];
			AVStream *ostream = output->streams[packet->stream_index];

			packet->pos = -1;
			packet->pts = av_rescale_q(packet->pts, istream->time_base, ostream->time_base);
			packet->dts = av_rescale_q(packet->dts, istream->time_base, ostream->time_base);
			packet->duration = av_rescale_q(packet->duration, istream->time_base, ostream->time_base);

			error::handle_ffmpeg_error(
				av_interleaved_write_frame(output, packet)
			);

			av_packet_unref(packet);
		}

		av_packet_free(&packet);

		av_write_trailer(output);
		avformat_free_context(output);
	}
}

