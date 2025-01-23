#include "src/constants.hh"
#include "src/eval/eobject.hh"
#include "src/filter/params.hh"
#include "src/muxing/error.hh"
#include "src/filter/filter.hh"
#include "src/muxing.hh"

extern "C" {
	#include "libavcodec/codec_par.h"
	#include "libavcodec/packet.h"
	#include "libavformat/avformat.h"
	#include "libavutil/error.h"
}

namespace vcat::muxing {
	void write_output(Spanned<const vcat::EObject&> eobject, const shared::Parameters& params) {
		vcat::Span span = eobject.span;

		const vcat::filter::VFilter *filter = dynamic_cast<const vcat::filter::VFilter *>(&eobject.val);

		if(!filter) {
			throw error::invalid_output(span);
		}

		;

		filter::FilterContext ctx = filter::VideoParameters {
			.width = params.width,
			.height=params.height,
		};

		std::unique_ptr<filter::PacketSource> source;

		if(params.lossless) {
			source = filter->get_pkts(ctx, span);
		} else {
			source = encode(ctx, span, *filter);
		}

		const AVCodecParameters *ivcodec = source->video_codec();

		AVFormatContext *output = nullptr;
		error::handle_ffmpeg_error(
			avformat_alloc_output_context2(&output, nullptr, nullptr, "output.mp4")
		);

		AVStream *ostream = avformat_new_stream(output, nullptr);
		ostream->time_base = constants::TIMEBASE;

		error::handle_ffmpeg_error(
			ostream ? 0 : AVERROR_UNKNOWN
		);

		error::handle_ffmpeg_error(
			avcodec_parameters_copy(ostream->codecpar, ivcodec)
		);

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
			AVStream *ostream = output->streams[packet->stream_index];

			packet->pos = -1;
			av_packet_rescale_ts(packet, ostream->time_base, constants::TIMEBASE);

			error::handle_ffmpeg_error(
				av_interleaved_write_frame(output, packet)
			);

			av_packet_unref(packet);
		}

		av_packet_free(&packet);

		av_write_trailer(output);

		if (!(output->oformat->flags & AVFMT_NOFILE))
			avio_closep(&output->pb);

		avformat_free_context(output);
	}
}

