/******************************************************************************/
/*!
\file           VideoFrameQueue.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon], 100%
\brief          Declares a thread-safe queue used to pass decoded video frames
                from FFmpeg to render-time playback systems.

                Defines the frame payload structure along with synchronized push,
                peek, pop, and clear operations.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once

#include <cstdint>
#include <mutex>
#include <queue>
#include <vector>


namespace Video {

struct FrameData {
  int width{0};
  int height{0};
  std::vector<uint8_t> rgbPixels{};
  double pts{0.0};
};

class VideoFrameQueue {
public:
  void push(FrameData &&frame) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.push(std::move(frame));
  }

  bool tryPeek(FrameData &outFrame) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_queue.empty()) {
      return false;
    }
    outFrame = m_queue.front();
    return true;
  }

  bool tryPop(FrameData &outFrame) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_queue.empty()) {
      return false;
    }
    outFrame = std::move(m_queue.front());
    m_queue.pop();
    return true;
  }

  void clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::queue<FrameData> empty;
    std::swap(m_queue, empty);
  }

  size_t size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
  }

private:
  std::queue<FrameData> m_queue;
  mutable std::mutex m_mutex;
};

} // namespace Video
