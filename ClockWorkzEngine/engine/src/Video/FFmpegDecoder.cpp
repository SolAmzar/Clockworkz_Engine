#include "Video/FFmpegDecoder.hpp"
#include <chrono>
#include <iostream>


extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

}

namespace Video {

FFmpegDecoder::FFmpegDecoder() {}

FFmpegDecoder::~FFmpegDecoder() {
  stopWorkerThread();
  cleanup();
}

void FFmpegDecoder::cleanup() {
  if (m_swsContext) {
    sws_freeContext(m_swsContext);
    m_swsContext = nullptr;
  }
  if (m_rgbFrame) {
    av_freep(&m_rgbFrame->data[0]);
    av_frame_free(&m_rgbFrame);
  }
  if (m_frame) {
    av_frame_free(&m_frame);
  }
  if (m_packet) {
    av_packet_free(&m_packet);
  }
  if (m_codecContext) {
    avcodec_free_context(&m_codecContext);
  }
  if (m_formatContext) {
    avformat_close_input(&m_formatContext);
  }
}

bool FFmpegDecoder::load(const std::string &path) {
  stopWorkerThread();
  cleanup();

  m_filepath = path;
  m_state = State::Stopped;
  m_currentTime = 0.0;
  m_frameQueue.clear();

  m_formatContext = avformat_alloc_context();
  if (avformat_open_input(&m_formatContext, m_filepath.c_str(), nullptr,
                          nullptr) != 0) {
    std::cerr << "FFmpegDecoder: Could not open file " << m_filepath << "\n";
    m_state = State::Error;
    return false;
  }

  if (avformat_find_stream_info(m_formatContext, nullptr) < 0) {
    std::cerr << "FFmpegDecoder: Could not get stream info\n";
    m_state = State::Error;
    return false;
  }

  m_videoStreamIndex = -1;
  const AVCodec *decoder = nullptr;

  for (unsigned int i = 0; i < m_formatContext->nb_streams; i++) {
    const AVCodecParameters *params = m_formatContext->streams[i]->codecpar;
    if (params->codec_type == AVMEDIA_TYPE_VIDEO) {
      m_videoStreamIndex = i;
      decoder = avcodec_find_decoder(params->codec_id);
      break;
    }
  }

  if (m_videoStreamIndex == -1 || !decoder) {
    std::cerr << "FFmpegDecoder: Could not find video stream or decoder\n";
    m_state = State::Error;
    return false;
  }

  AVStream *stream = m_formatContext->streams[m_videoStreamIndex];
  m_codecContext = avcodec_alloc_context3(decoder);
  avcodec_parameters_to_context(m_codecContext, stream->codecpar);

  if (avcodec_open2(m_codecContext, decoder, nullptr) < 0) {
    std::cerr << "FFmpegDecoder: Could not open codec\n";
    m_state = State::Error;
    return false;
  }

  m_width = m_codecContext->width;
  m_height = m_codecContext->height;

  if (stream->avg_frame_rate.den != 0) {
    m_fps = static_cast<float>(stream->avg_frame_rate.num) /
            static_cast<float>(stream->avg_frame_rate.den);
  }

  if (stream->time_base.den != 0) {
    m_timeBase = av_q2d(stream->time_base);
  } else {
    m_timeBase = 1.0 / m_fps; // Fallback
  }

  m_duration = static_cast<double>(m_formatContext->duration) / AV_TIME_BASE;

  m_frame = av_frame_alloc();
  m_rgbFrame = av_frame_alloc();
  m_packet = av_packet_alloc();

  int numBytes =
      av_image_get_buffer_size(AV_PIX_FMT_RGBA, m_width, m_height, 1);
  uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
  av_image_fill_arrays(m_rgbFrame->data, m_rgbFrame->linesize, buffer,
                       AV_PIX_FMT_RGBA, m_width, m_height, 1);

  m_swsContext = sws_getContext(m_width, m_height, m_codecContext->pix_fmt,
                                m_width, m_height, AV_PIX_FMT_RGBA,
                                SWS_BILINEAR, nullptr, nullptr, nullptr);

  m_state = State::Stopped;
  return true;
}

void FFmpegDecoder::startWorkerThread() {
  if (m_state == State::Error || m_videoStreamIndex == -1)
    return;

  m_stopThread = false;
  m_workerThread = std::thread(&FFmpegDecoder::decodeLoop, this);
}

void FFmpegDecoder::stopWorkerThread() {
  m_stopThread = true;
  if (m_workerThread.joinable()) {
    m_workerThread.join();
  }
}

void FFmpegDecoder::play() {
  if (m_state == State::Stopped || m_state == State::Finished) {
    m_frameQueue.clear();
    m_state = State::Playing;
  } else if (m_state == State::Paused) {
    m_state = State::Playing;
  }
}

void FFmpegDecoder::pause() {
  if (m_state == State::Playing) {
    m_state = State::Paused;
  }
}

void FFmpegDecoder::stop() {
  m_state = State::Stopped;
  m_seekTarget = 0.0;
  m_seekRequested = true;
}

void FFmpegDecoder::restart() {
  stop();
  play();
}

void FFmpegDecoder::setPlaybackSpeed(float multiplier) {
  m_playbackSpeed = multiplier;
}

void FFmpegDecoder::seek(float timeInSeconds) {
  m_seekTarget = static_cast<double>(timeInSeconds);
  m_seekRequested = true;
}

float FFmpegDecoder::getDuration() const {
  return static_cast<float>(m_duration);
}
float FFmpegDecoder::getCurrentTime() const {
  return static_cast<float>(m_currentTime.load());
}
int FFmpegDecoder::getWidth() const { return m_width; }
int FFmpegDecoder::getHeight() const { return m_height; }
float FFmpegDecoder::getFps() const { return m_fps; }

std::string FFmpegDecoder::getPlaybackState() const {
  switch (m_state.load()) {
  case State::Stopped:
    return "Stopped";
  case State::Playing:
    return "Playing";
  case State::Paused:
    return "Paused";
  case State::Finished:
    return "Finished";
  case State::Error:
    return "Error";
  }
  return "Unknown";
}

bool FFmpegDecoder::readFrameInternal(FrameData &outFrame) {
  while (av_read_frame(m_formatContext, m_packet) >= 0) {
    if (m_packet->stream_index == m_videoStreamIndex) {
      int response = avcodec_send_packet(m_codecContext, m_packet);
      if (response < 0) {
        av_packet_unref(m_packet);
        continue;
      }

      response = avcodec_receive_frame(m_codecContext, m_frame);
      if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
        av_packet_unref(m_packet);
        continue;
      } else if (response < 0) {
        av_packet_unref(m_packet);
        return false;
      }

      // We got a frame! Convert to RGBA
      sws_scale(m_swsContext, m_frame->data, m_frame->linesize, 0, m_height,
                m_rgbFrame->data, m_rgbFrame->linesize);

      outFrame.width = m_width;
      outFrame.height = m_height;
      const int rowBytes = m_width * 4;
      outFrame.rgbPixels.resize(static_cast<size_t>(rowBytes) *
                                static_cast<size_t>(m_height));
      for (int y = 0; y < m_height; ++y) {
        const uint8_t *src = m_rgbFrame->data[0] +
                             static_cast<size_t>(y) * m_rgbFrame->linesize[0];
        uint8_t *dst = outFrame.rgbPixels.data() +
                       static_cast<size_t>(y) * rowBytes;
        std::copy(src, src + rowBytes, dst);
      }
      outFrame.pts = m_frame->pts * m_timeBase;
      m_currentTime = outFrame.pts;

      av_packet_unref(m_packet);
      return true;
    }
    av_packet_unref(m_packet);
  }
  return false; // EOF
}

void FFmpegDecoder::decodeLoop() {
  // Basic decode loop logic
  while (!m_stopThread) {
    if (m_state == State::Stopped || m_state == State::Error ||
        m_state == State::Finished) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    if (m_seekRequested) {
      int64_t targetPts =
          static_cast<int64_t>(m_seekTarget.load() / m_timeBase);
      avformat_seek_file(m_formatContext, m_videoStreamIndex, INT64_MIN,
                         targetPts, INT64_MAX, AVSEEK_FLAG_BACKWARD);
      avcodec_flush_buffers(m_codecContext);
      m_frameQueue.clear();
      m_currentTime = m_seekTarget.load();
      m_seekRequested = false;
    }

    // Limit queue size to avoid filling up RAM
    if (m_frameQueue.size() >= 10) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      continue;
    }

    FrameData fd;
    if (readFrameInternal(fd)) {
      // Push frame to queue. The playback system will check timestamps and
      // decide when to pull. But we must update currentTime somewhere.
      // Generally, the consumer (VideoPlaybackSystem) tells us what the ideal
      // time is, or we just push frames. For now we just push.
      m_frameQueue.push(std::move(fd));
    } else {
      // EOF
      m_state = State::Finished;
    }
  }
}

} // namespace Video
