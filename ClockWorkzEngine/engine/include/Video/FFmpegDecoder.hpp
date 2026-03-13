/******************************************************************************/
/*!
\file           FFmpegDecoder.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon], 100%
\brief          Declares the FFmpeg-backed video decoder used by engine video
                playback.

                Manages FFmpeg decode state, a background decode thread, and
                frame delivery into a thread-safe queue.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once

#include "VideoFrameQueue.hpp"
#include <atomic>
#include <string>
#include <thread>

// Forward declarations for FFmpeg structs to avoid including headers here
struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

namespace Video {

class FFmpegDecoder {
public:
  FFmpegDecoder();
  ~FFmpegDecoder();

  bool load(const std::string &path);
  void play();
  void pause();
  void stop();
  void restart();
  void setPlaybackSpeed(float multiplier);
  void seek(float timeInSeconds);

  // Threading
  void startWorkerThread();
  void stopWorkerThread();

  // Status metrics
  float getDuration() const;
  float getCurrentTime()
      const; // Sync logic can be based on system timers during playback
  int getWidth() const;
  int getHeight() const;
  float getFps() const;
  std::string getPlaybackState() const;
  std::string getFilepath() const { return m_filepath; }

  // Output queue
  VideoFrameQueue &getFrameQueue() { return m_frameQueue; }

private:
  void cleanup();
  void decodeLoop();
  bool readFrameInternal(FrameData &outFrame);

  AVFormatContext *m_formatContext{nullptr};
  AVCodecContext *m_codecContext{nullptr};
  SwsContext *m_swsContext{nullptr};
  AVFrame *m_frame{nullptr};
  AVFrame *m_rgbFrame{nullptr};
  AVPacket *m_packet{nullptr};

  int m_videoStreamIndex{-1};

  std::string m_filepath;

  enum class State { Stopped, Playing, Paused, Finished, Error };
  std::atomic<State> m_state{State::Stopped};

  std::atomic<float> m_playbackSpeed{1.0f};

  // Video Properties
  int m_width{0};
  int m_height{0};
  float m_fps{0.0f};
  double m_duration{0.0};
  double m_timeBase{0.0};

  // Queue (Thread-safe)
  VideoFrameQueue m_frameQueue;

  // Threading
  std::thread m_workerThread;
  std::atomic<bool> m_stopThread{false};
  std::atomic<bool> m_seekRequested{false};
  std::atomic<double> m_seekTarget{0.0};

  // Current logical playback time
  std::atomic<double> m_currentTime{0.0};
};

} // namespace Video
