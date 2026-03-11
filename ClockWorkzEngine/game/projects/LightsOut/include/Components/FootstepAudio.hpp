#pragma once

#include <string>

namespace Components {

struct FootstepAudio {
  float interval{0.6f};
  float sprintIntervalScale{0.6f};
  float sprintSpeedThreshold{14.0f};
  float minSpeed{0.1f};
  float resetDelay{0.05f};
  float volume{1.0f};
  int variationCount{4};
  std::string soundPrefix{"fs_concrete_"};
  float timerRemaining{0.0f};
};

} // namespace Components
