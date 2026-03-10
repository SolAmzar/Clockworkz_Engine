/******************************************************************************/
/*!
\file           AudioManager.hpp
\project        Lights Out
\author(s)      [Amzar]
\brief          Handles Audio using FMOD API.
                Responsibilities include:
                - Initializing and shutting down the FMOD system
                - Loading and unloading sound resources
                - Playing sounds with options for volume, looping, and grouping
(BGM, SFX)
                - Managing audio channels and groups for volume control and
pausing
                - Providing 3D audio listener settings if needed

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "Math/Vec3.hpp"
#include <memory>
#include <string>
#include <unordered_map>

// Forward declare FMOD to avoid heavy headers leaking
namespace FMOD {
class System;
class Sound;
class Channel;
class ChannelGroup;
} // namespace FMOD

namespace Audio {

enum class Group { Master, BGM, SFX };

struct SoundDesc {
  std::string name; // unique id
  std::string path = "";
  bool loop = false;
  bool stream = false; // stream large bgm from disk
  bool is3D = false;
};

class AudioManager {
public:
  AudioManager() = default;
  ~AudioManager();

  bool init(int maxChannels = 64);
  void shutdown();
  void update(double dt);

  // Resources
  bool load(const SoundDesc &desc);
  void unload(const std::string &id);
  void unloadAll();

  // Playback
  int play(const std::string &id, Group group, float volume, bool paused);
  void playOneShot(const std::string &id, Group group = Group::SFX,
                   float volume = 1.0f);

  // Stop control
  void stop(std::string const &name); // stop a specific playing sound by key
  void stopGroup(Group g);            // stop everything in a group
  void stopAll();                     // panic-stop everything
  void fadeOutChannel(int channel, float duration);
  void fadeOutActive(float duration);

  // Channel control
  void stop(int channel);
  void setPaused(int channel, bool paused);
  void setVolume(int channel, float volume);
  void setPitch(int channel, float pitch);

  // Groups
  void setGroupVolume(Group g, float volume);
  void setGroupPaused(Group g, bool paused);

  // Listener (for 3D if you want it)
  void setListener(const vec3 &pos, const vec3 &vel, const vec3 &forward,
                   const vec3 &up);

  // Set 3D attributes for a specific playing channel
  void setChannel3DPosition(int channelId, const vec3 &pos,
                            const vec3 &vel = vec3(0.0f, 0.0f, 0.0f));
  void setChannel3DDistance(int channelId, float minDistance,
                            float maxDistance);
  void setChannel3DCone(int channelId, const vec3 &forward,
                        float innerAngleDeg, float outerAngleDeg,
                        float outerVolume);

  bool isReady() const noexcept { return m_system != nullptr; }
  bool isChannelPlaying(int channel) const;

  FMOD::System *getSystem() const { return m_system; }
  static AudioManager &get();

  int findChannel(const std::string &id);
  bool isPlaying(const std::string &id);

  // Data-driven support
  struct SoundProfile {
    float volume = 1.0f;
    float pitch = 1.0f;
    Group group = Group::SFX;
  };
  void registerSoundProfile(const std::string &name,
                            const SoundProfile &profile);
  void unregisterSoundProfile(const std::string &name);
  float getSoundProfileVolume(const std::string &name) const;
  void setSoundProfileVolume(const std::string &name, float volume);
  int play(const std::string &name); // Overload for playing by profile name
                                     // containing defaults
  int play(const std::string &name, float volume,
           float pitch = 1.0f); // Overload for manual override

private:
  struct ChannelHandle {
    FMOD::Channel *ch = nullptr;
    std::string soundId{};
    Group group = Group::SFX;
  };
  struct FadeOut {
    int channel = 0;
    float startVolume = 1.0f;
    float duration = 0.0f;
    float elapsed = 0.0f;
  };

  FMOD::System *m_system = nullptr;
  FMOD::ChannelGroup *m_masterGroup = nullptr;
  FMOD::ChannelGroup *m_bgmGroup = nullptr;
  FMOD::ChannelGroup *m_sfxGroup = nullptr;

  std::unordered_map<std::string, SoundProfile> m_soundProfiles;
  std::unordered_map<std::string, FMOD::Sound *> m_sounds;
  std::unordered_map<int, ChannelHandle> m_channels;
  std::unordered_map<int, FadeOut> m_fadeOuts;
  int m_nextChannelId = 1;

  FMOD::ChannelGroup *groupHandle(Group g) const;
  bool createSound(SoundDesc const &desc, FMOD::Sound **out);
};

} // namespace Audio


