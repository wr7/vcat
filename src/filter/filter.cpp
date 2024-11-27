#include "src/filter/filter.hh"
#include "src/error.hh"
#include "src/filter/error.hh"
#include <cassert>
#include <cstring>
#include <format>
#include <memory>
#include <sstream>
#include <fstream>
#include <string>
#include <string_view>

extern "C" {
	#include <libavformat/avformat.h>
	#include <libavutil/error.h>
}

namespace dvel::filter {
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

		SHA256 hasher;

		while(f.good()) {
			size_t num_bytes = f.read(&buf[0], sizeof(buf)).gcount();

			hasher.add(&buf[0], num_bytes);
		}

		hasher.getHash(m_file_hash);

		if(f.bad()) {
			throw std::format("Failed to open file `{}`: {}", m_path, strerror(errno));
		}

		f.close();
	}

	std::string VideoFile::to_string() const {
		std::stringstream s;
		s << "VideoFile(\""
			<< PartiallyEscaped(m_path)
			<< "\")";

		return s.str();
	}

	std::string VideoFile::type_name() const {
		return "VideoFile";
	}

	class VideoFileSource : public PacketSource {
		public:
			VideoFileSource() = delete;

			VideoFileSource(std::string_view path, Span span)
				: m_ctx(NULL) 
				, m_span(span) {
				error::handle_ffmpeg_error(m_span,
					avformat_open_input(&m_ctx, path.data(), NULL, NULL)
				);

				assert(m_ctx);
			}

			bool next_pkt(AVPacket *packet) {
				int ret_code = av_read_frame(m_ctx, packet);
				if(ret_code == AVERROR_EOF) {
					return false;
				}

				error::handle_ffmpeg_error(m_span, ret_code);

				return true;
			}

			~VideoFileSource() {
				avformat_close_input(&m_ctx);
			}

		private:
			AVFormatContext *m_ctx;
			Span             m_span;
	};

	std::unique_ptr<PacketSource> VideoFile::get_pkts(Span s) const {
		return std::make_unique<VideoFileSource>(m_path, s);
	}
}
