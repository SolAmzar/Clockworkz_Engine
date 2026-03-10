/******************************************************************************/
/*!
\file           AudioManager.cpp
\project        Lights Out
\author(s)      [Amzar] , 100%
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

#include "Audio/AudioManager.hpp"
#include "Assets/SoundManager.hpp"
#include <algorithm>
#include <cassert>
#include <fmod.hpp>
#include <fmod_errors.h>
#include <iostream>

using namespace Audio;

static bool FMOD_CHECK(FMOD_RESULT r, const char *what) {
  if (r != FMOD_OK) {
    std::cerr << "[FMOD] " << what << " failed: " << FMOD_ErrorString(r)
              << "\n";
    return false;
  }
  return true;
}

static bool isChannelAlive(FMOD::Channel *ch) {
  if (!ch)
    return false;
  bool playing = false;
  if (ch->isPlaying(&playing) != FMOD_OK)
    return false;
  return playing;
}

int AudioManager::findChannel(const std::string &id) {
  // Walk all tracked channels.
  // While we're here, also prune any dead ones so the map stays clean.
  for (auto it = m_channels.begin(); it != m_channels.end();) {
    FMOD::Channel *ch = it->second.ch;

    if (!isChannelAlive(ch)) {
      // Channel finished or invalid ? erase.
      it = m_channels.erase(it);
      continue;
    }

    if (it->second.soundId == id) {
      // Found a live channel playing this sound id.
      return it->first; // this is the channel handle
    }

    ++it;
  }

  // 0 means "no live channel found" (consistent with play() failure)
  std::cerr << "[Audio] Channel " << id << " not found" << "\n";
  return 0;
}

bool AudioManager::isChannelPlaying(int channel) const {
  auto it = m_channels.find(channel);
  if (it == m_channels.end())
    return false;
  return isChannelAlive(it->second.ch);
}
bool AudioManager::isPlaying(const std::string &id) {
  return findChannel(id) != 0;
}

AudioManager::~AudioManager() { shutdown(); }

AudioManager &AudioManager::get() {
  static AudioManager instance; // <--- created once, destroyed once
  return instance;
}

bool AudioManager::init(int maxChannels) {
  std::cout << "[DEBUG] AudioManager::init called\n";
  if (m_system)
    return true;
  if (!FMOD_CHECK(FMOD::System_Create(&m_system), "System_Create"))
    return false;
  if (!FMOD_CHECK(m_system->init(maxChannels, FMOD_INIT_NORMAL, nullptr),
                  "System::init"))
    return false;

  // Channel groups
  FMOD_CHECK(m_system->getMasterChannelGroup(&m_masterGroup),
             "getMasterChannelGroup");
  FMOD_CHECK(m_system->createChannelGroup("BGM", &m_bgmGroup),
             "createChannelGroup BGM");
  FMOD_CHECK(m_system->createChannelGroup("SFX", &m_sfxGroup),
             "createChannelGroup SFX");
  if (m_masterGroup) {
    m_masterGroup->addGroup(m_bgmGroup);
    m_masterGroup->addGroup(m_sfxGroup);
  }
  return true;
}

void AudioManager::shutdown() {
  std::cout << "[DEBUG] AudioManager::shutdown called\n";
  unloadAll();
  if (m_bgmGroup) {
    m_bgmGroup->release();
    m_bgmGroup = nullptr;
  }
  if (m_sfxGroup) {
    m_sfxGroup->release();
    m_sfxGroup = nullptr;
  }
  if (m_system) {
    m_system->close();
    m_system->release();
    m_system = nullptr;
    m_masterGroup = nullptr;
  }
  m_channels.clear();
  m_nextChannelId = 1;
}

void AudioManager::update(double dt) {
  if (m_system)
    m_system->update();
  // Process fade-outs
  for (auto it = m_fadeOuts.begin(); it != m_fadeOuts.end();) {
    auto chIt = m_channels.find(it->first);
    if (chIt == m_channels.end() || !isChannelAlive(chIt->second.ch)) {
      it = m_fadeOuts.erase(it);
      continue;
    }

    FadeOut &fade = it->second;
    fade.elapsed += static_cast<float>(dt);
    float t = (fade.duration <= 0.0f) ? 1.0f : (fade.elapsed / fade.duration);
    if (t > 1.0f)
      t = 1.0f;
    float newVol = fade.startVolume * (1.0f - t);
    chIt->second.ch->setVolume(newVol);

    if (t >= 1.0f) {
      chIt->second.ch->stop();
      m_channels.erase(chIt);
      it = m_fadeOuts.erase(it);
      continue;
    }

    ++it;
  }

  for (auto it = m_channels.begin(); it != m_channels.end();) {
    FMOD::Channel *ch = it->second.ch;
    if (!isChannelAlive(ch))
      it = m_channels.erase(it);
    else
      ++it;
  }
}

bool AudioManager::createSound(const SoundDesc &d, FMOD::Sound **out) {
  if (!m_system)
    return false;
  FMOD_MODE mode = FMOD_DEFAULT | (d.loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
  mode |= d.is3D ? FMOD_3D : FMOD_2D;

  FMOD_RESULT r =
      d.stream ? m_system->createStream(d.path.c_str(), mode, nullptr, out)
               : m_system->createSound(d.path.c_str(), mode, nullptr, out);

  return FMOD_CHECK(r, ("createSound " + d.path).c_str());
}

bool AudioManager::load(const SoundDesc &desc) {
  if (m_sounds.find(desc.name) != m_sounds.end())
    return true;
  FMOD::Sound *s = nullptr;
  if (!createSound(desc, &s))
    return false;
  m_sounds[desc.name] = s;
  return true;
}

void AudioManager::unload(const std::string &id) {
  auto it = m_sounds.find(id);
  if (it != m_sounds.end()) {
    if (it->second)
      it->second->release();
    m_sounds.erase(it);
  }
}

void AudioManager::unloadAll() {
  for (auto &kv : m_sounds) {
    if (kv.second)
      kv.second->release();
  }
  m_sounds.clear();
}

FMOD::ChannelGroup *AudioManager::groupHandle(Group g) const {
  switch (g) {
  case Group::BGM:
    return m_bgmGroup;
  case Group::SFX:
    return m_sfxGroup;
  default:
    return m_masterGroup;
  }
}

int AudioManager::play(const std::string &id, Group g, float volume,
                       bool paused) {
  if (!m_system)
    return 0;

  auto clip = Assets::SoundManager::Get(id);
  if (!clip) {
    std::cerr << "[Audio] Sound not loaded via SoundManager: " << id << "\n";
    return 0;
  }

  FMOD::Sound *sound = clip->GetRaw();
  if (!sound) {
    std::cerr << "[Audio] AudioClip has null sound: " << id << "\n";
    return 0;
  }

  FMOD::Channel *ch = nullptr;
  FMOD_RESULT r = m_system->playSound(sound, groupHandle(g), true, &ch);
  if (r != FMOD_OK) {
    std::cerr << "[FMOD] playSound('" << id
              << "') failed: " << FMOD_ErrorString(r) << "\n";
    return 0;
  }

  if (ch) {
    ch->setVolume(volume);
    ch->setChannelGroup(groupHandle(g));
    ch->setPaused(paused);

    int handle = m_nextChannelId++;
    m_channels[handle] = ChannelHandle{ch, id, g};
    if (!isChannelAlive(ch)) {
      m_channels.erase(handle);
    }
    return handle;
  }

  return 0;
}

void AudioManager::registerSoundProfile(const std::string &name,
                                        const SoundProfile &profile) {
  m_soundProfiles[name] = profile;
}

void AudioManager::unregisterSoundProfile(const std::string &name) {
  m_soundProfiles.erase(name);
}

float AudioManager::getSoundProfileVolume(const std::string &name) const {
  auto it = m_soundProfiles.find(name);
  if (it != m_soundProfiles.end()) {
    return it->second.volume;
  }
  return 1.0f;
}

void AudioManager::setSoundProfileVolume(const std::string &name,
                                         float volume) {
  auto it = m_soundProfiles.find(name);
  if (it == m_soundProfiles.end()) {
    SoundProfile profile;
    profile.volume = volume;
    profile.pitch = 1.0f;
    profile.group = Group::SFX;
    m_soundProfiles[name] = profile;
    return;
  }
  it->second.volume = volume;
}

int AudioManager::play(const std::string &name) {
  auto it = m_soundProfiles.find(name);
  if (it != m_soundProfiles.end()) {
    const auto &p = it->second;
    // Defaults: pitch is not supported in the main play() yet?
    // Wait, main play() takes (id, group, volume, paused).
    // It does NOT take pitch.
    // If I need pitch, I must add pitch support to the main play() or handle it
    // here.
    int h = play(name, p.group, p.volume, false);

    // Apply pitch if handle is valid
    if (h > 0 && p.pitch != 1.0f) {
      auto chIt = m_channels.find(h);
      if (chIt != m_channels.end() && chIt->second.ch) {
        chIt->second.ch->setPitch(p.pitch);
      }
    }
    return h;
  }

  // Fallback
  return play(name, Group::SFX, 1.0f, false);
}

int AudioManager::play(const std::string &name, float volume, float pitch) {
  // We still want to get the group from the profile if possible,
  // otherwise default to SFX.
  Group group = Group::SFX;
  auto it = m_soundProfiles.find(name);
  if (it != m_soundProfiles.end()) {
    group = it->second.group;
  }

  int h = play(name, group, volume, false);

  // Apply pitch if handle is valid
  if (h > 0 && pitch != 1.0f) {
    auto chIt = m_channels.find(h);
    if (chIt != m_channels.end() && chIt->second.ch) {
      chIt->second.ch->setPitch(pitch);
    }
  }
  return h;
}

void AudioManager::playOneShot(const std::string &id, Group g, float volume) {
  int h = play(id, g, volume, false);
  // nothing special: one-shot just doesn't track or pause later
  (void)h;
}

void AudioManager::stop(int h) {
  auto it = m_channels.find(h);
  if (it == m_channels.end())
    return;
  FMOD::Channel *ch = it->second.ch;
  if (ch) {
    ch->stop();
    m_channels.erase(it);
  }
}

void AudioManager::stop(std::string const &name) {
  for (auto it = m_channels.begin(); it != m_channels.end();) {
    if (it->second.soundId == name) {
      if (it->second.ch)
        it->second.ch->stop();
      it = m_channels.erase(it);
    } else {
      ++it;
    }
  }
}

void AudioManager::stopGroup(Group g) {
  if (auto *grp = groupHandle(g)) {
    grp->stop(); // stops all channels in that group
  }
  // also clear any cached handles from that group
  for (auto it = m_channels.begin(); it != m_channels.end();) {
    if (it->second.group == g)
      it = m_channels.erase(it);
    else
      ++it;
  }
}

void AudioManager::stopAll() {
  if (m_masterGroup)
    m_masterGroup->stop(); // nukes everything
  m_channels.clear();
  m_fadeOuts.clear();
}

void AudioManager::fadeOutChannel(int channel, float duration) {
  auto it = m_channels.find(channel);
  if (it == m_channels.end() || !isChannelAlive(it->second.ch))
    return;
  if (m_fadeOuts.find(channel) != m_fadeOuts.end())
    return;

  float currentVolume = 1.0f;
  it->second.ch->getVolume(&currentVolume);
  FadeOut fade;
  fade.channel = channel;
  fade.startVolume = currentVolume;
  fade.duration = duration;
  fade.elapsed = 0.0f;
  m_fadeOuts[channel] = fade;
}

void AudioManager::fadeOutActive(float duration) {
  for (const auto &kv : m_channels) {
    fadeOutChannel(kv.first, duration);
  }
}

void AudioManager::setPaused(int h, bool paused) {
  auto it = m_channels.find(h);
  if (it != m_channels.end() && it->second.ch) {
    it->second.ch->setPaused(paused);
  }
}

void AudioManager::setVolume(int h, float volume) {
  auto it = m_channels.find(h);
  if (it != m_channels.end() && it->second.ch) {
    it->second.ch->setVolume(volume);
  }
}

void AudioManager::setPitch(int h, float pitch) {
  auto it = m_channels.find(h);
  if (it != m_channels.end() && it->second.ch) {
    it->second.ch->setPitch(pitch);
  }
}

void AudioManager::setGroupVolume(Group g, float volume) {
  if (auto *grp = groupHandle(g))
    grp->setVolume(volume);
}

void AudioManager::setGroupPaused(Group g, bool paused) {
  if (auto *grp = groupHandle(g))
    grp->setPaused(paused);
}

void AudioManager::setListener(const vec3 &pos, const vec3 &vel,
                               const vec3 &forward, const vec3 &up) {
  if (!m_system)
    return;
  FMOD_VECTOR p{pos.x, pos.y, pos.z};
  FMOD_VECTOR v{vel.x, vel.y, vel.z};
  FMOD_VECTOR f{forward.x, forward.y, forward.z};
  FMOD_VECTOR u{up.x, up.y, up.z};
  m_system->set3DListenerAttributes(0, &p, &v, &f, &u);
}

void AudioManager::setChannel3DPosition(int channelId, const vec3 &pos,
                                        const vec3 &vel) {
  auto it = m_channels.find(channelId);
  if (it != m_channels.end() && it->second.ch) {
    FMOD_VECTOR p{pos.x, pos.y, pos.z};
    FMOD_VECTOR v{vel.x, vel.y, vel.z};
    it->second.ch->set3DAttributes(&p, &v);
  }
}


void AudioManager::setChannel3DDistance(int channelId, float minDistance,
                                        float maxDistance) {
  auto it = m_channels.find(channelId);
  if (it == m_channels.end() || !it->second.ch)
    return;

  const float safeMin = std::max(0.0f, minDistance);
  const float safeMax = std::max(safeMin, maxDistance);
  it->second.ch->set3DMinMaxDistance(safeMin, safeMax);
}

void AudioManager::setChannel3DCone(int channelId, const vec3 &forward,
                                    float innerAngleDeg, float outerAngleDeg,
                                    float outerVolume) {
  auto it = m_channels.find(channelId);
  if (it == m_channels.end() || !it->second.ch)
    return;

  vec3 dir = normalize(forward);
  if (length_sq(dir) <= 0.0f) {
    dir = vec3(1.0f, 0.0f, 0.0f);
  }

  const float safeInner = std::clamp(innerAngleDeg, 0.0f, 360.0f);
  const float safeOuter = std::clamp(outerAngleDeg, safeInner, 360.0f);
  const float safeOuterVolume = std::clamp(outerVolume, 0.0f, 1.0f);

  FMOD_VECTOR orientation{dir.x, dir.y, dir.z};
  it->second.ch->set3DConeOrientation(&orientation);
  it->second.ch->set3DConeSettings(safeInner, safeOuter, safeOuterVolume);
}
// int AudioManager::play(const std::string& id, Group g, float volume, bool
// paused) {
//     auto it = m_sounds.find(id);
//
//     if (it == m_sounds.end()) {
//         std::cerr << "[Audio] Sound not loaded: " << id << "\n";
//         return 0;
//     }
//
//     if (it->second == nullptr) {
//         std::cerr << "[Audio] Sound* is null: " << id << "\n";
//         return 0;
//     }
//
//
//     FMOD::Channel* ch = nullptr;
//     if (!FMOD_CHECK(m_system->playSound(it->second, groupHandle(g), true,
//     &ch), "playSound")) return 0; if (ch) {
//         ch->setVolume(volume);
//         ch->setChannelGroup(groupHandle(g));
//         ch->setPaused(paused);
//     }
//     int handle = m_nextChannelId++;
//     m_channels[handle] = { ch };
//     return handle;
// }


