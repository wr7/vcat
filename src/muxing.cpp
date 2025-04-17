#include "src/constants.hh"
#include "src/eval/eobject.hh"
#include "src/filter/params.hh"
#include "src/muxing/error.hh"
#include "src/filter/filter.hh"
#include "src/muxing/util.hh"
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

		filter::FilterContext ctx {
			filter::VideoParameters {
				.width     = params.width,
				.height    = params.height,
				.fixed_fps = params.fixed_fps,
				.fps       = params.fps,
			},
			filter::AudioParameters {
				.sample_rate = static_cast<int>(params.sample_rate),
				.sample_format = params.sample_format,
				.channel_layout = constants::CHANNEL_LAYOUT,
			}
		};

		std::unique_ptr<filter::PacketSource> vsource;
		std::unique_ptr<filter::PacketSource> asource;

		if(params.lossless) {
			vsource = filter->get_pkts(ctx, filter::StreamType::Video, span);
		} else {
			vsource = encode(ctx, span, *filter, filter::StreamType::Video);
		}

		asource = encode(ctx, span, *filter, filter::StreamType::Audio);

		const AVCodecParameters *ivcodec = vsource->video_codec();
		const AVCodecParameters *iacodec = asource->video_codec();

		AVFormatContext *output = nullptr;
		error::handle_ffmpeg_error(
			avformat_alloc_output_context2(&output, nullptr, nullptr, "output.mp4")
		);

		AVStream *ovstream = avformat_new_stream(output, nullptr);

		AVStream *oastream = avformat_new_stream(output, nullptr);

		error::handle_ffmpeg_error(
			ovstream && oastream ? 0 : AVERROR(ENOMEM)
		);

		error::handle_ffmpeg_error(
			avcodec_parameters_copy(ovstream->codecpar, ivcodec)
		);
		error::handle_ffmpeg_error(
			avcodec_parameters_copy(oastream->codecpar, iacodec)
		);

		ovstream->time_base = constants::TIMEBASE;
		oastream->time_base = AVRational {1, ctx.aparams.sample_rate}; // Many audio players require the time base to be one sample

		if(!(output->flags & AVFMT_NOFILE)) {
			error::handle_ffmpeg_error(
				avio_open(&output->pb, "output.mp4", AVIO_FLAG_WRITE)
			);
		}

		error::handle_ffmpeg_error(
			avformat_write_header(output, nullptr)
		);

		PacketInterleaver interleaver(std::move(vsource), std::move(asource));
		AVPacket *packet = nullptr;

		while(interleaver.next(&packet)) {
			AVStream *ostream = output->streams[packet->stream_index];

			packet->pos = -1;
			av_packet_rescale_ts(packet, constants::TIMEBASE, ostream->time_base);

			error::handle_ffmpeg_error(
				av_write_frame(output, packet)
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

