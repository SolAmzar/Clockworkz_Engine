#include "Core/AudioManager.hpp"
#include <fmod.hpp>
#include <fmod_errors.h>
#include <cassert>
#include <iostream>

using namespace Audio;

static bool FMOD_CHECK(FMOD_RESULT r, const char* what) {
    if (r != FMOD_OK) {
        std::cerr << "[FMOD] " << what << " failed: " << FMOD_ErrorString(r) << "\n";
        return false;
    }
    return true;
}

AudioManager::~AudioManager() { shutdown(); }

bool AudioManager::init(int maxChannels) {
    if (m_system) return true;
    if (!FMOD_CHECK(FMOD::System_Create(&m_system), "System_Create")) return false;
    if (!FMOD_CHECK(m_system->init(maxChannels, FMOD_INIT_NORMAL, nullptr), "System::init")) return false;

    // Channel groups
    FMOD_CHECK(m_system->getMasterChannelGroup(&m_master), "getMasterChannelGroup");
    FMOD_CHECK(m_system->createChannelGroup("BGM", &m_bgm), "createChannelGroup BGM");
    FMOD_CHECK(m_system->createChannelGroup("SFX", &m_sfx), "createChannelGroup SFX");
    if (m_master) { m_master->addGroup(m_bgm); m_master->addGroup(m_sfx); }
    return true;
}

void AudioManager::shutdown() {
    unloadAll();
    if (m_bgm) { m_bgm->release(); m_bgm = nullptr; }
    if (m_sfx) { m_sfx->release(); m_sfx = nullptr; }
    if (m_system) {
        m_system->close();
        m_system->release();
        m_system = nullptr;
        m_master = nullptr;
    }
    m_channels.clear();
    m_nextChannelId = 1;
}

void AudioManager::update() {
    if (m_system) m_system->update();
}

bool AudioManager::createSound(const SoundDesc& d, FMOD::Sound** out) {
    if (!m_system) return false;
    FMOD_MODE mode = FMOD_DEFAULT | (d.loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
    mode |= d.is3D ? FMOD_3D : FMOD_2D;

    FMOD_RESULT r = d.stream
        ? m_system->createStream(d.path.c_str(), mode, nullptr, out)
        : m_system->createSound(d.path.c_str(), mode, nullptr, out);

    return FMOD_CHECK(r, ("createSound " + d.path).c_str());
}

bool AudioManager::load(const std::string& id, const SoundDesc& desc) {
    if (m_sounds.find(id) != m_sounds.end()) return true;
    FMOD::Sound* s = nullptr;
    if (!createSound(desc, &s)) return false;
    m_sounds[id] = s;
    return true;
}

void AudioManager::unload(const std::string& id) {
    auto it = m_sounds.find(id);
    if (it != m_sounds.end()) {
        if (it->second) it->second->release();
        m_sounds.erase(it);
    }
}

void AudioManager::unloadAll() {
    for (auto& kv : m_sounds) {
        if (kv.second) kv.second->release();
    }
    m_sounds.clear();
}

FMOD::ChannelGroup* AudioManager::groupHandle(Group g) const {
    switch (g) {
    case Group::BGM: return m_bgm;
    case Group::SFX: return m_sfx;
    default:         return m_master;
    }
}

int AudioManager::play(const std::string& id, Group g, float volume, bool paused) {
    auto it = m_sounds.find(id);
    if (it == m_sounds.end()) {
        std::cerr << "[Audio] Sound not loaded: " << id << "\n";
        return 0;
    }
    FMOD::Channel* ch = nullptr;
    if (!FMOD_CHECK(m_system->playSound(it->second, groupHandle(g), true, &ch), "playSound")) return 0;
    if (ch) {
        ch->setVolume(volume);
        ch->setChannelGroup(groupHandle(g));
        ch->setPaused(paused);
        ch->setPaused(false);
    }
    int handle = m_nextChannelId++;
    m_channels[handle] = { ch };
    return handle;
}

void AudioManager::playOneShot(const std::string& id, Group g, float volume) {
    (void)play(id, g, volume, false);
}

void AudioManager::stop(int h) {
    auto it = m_channels.find(h);
    if (it != m_channels.end() && it->second.ch) {
        it->second.ch->stop();
        m_channels.erase(it);
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

void AudioManager::setGroupVolume(Group g, float volume) {
    if (auto* grp = groupHandle(g)) grp->setVolume(volume);
}

void AudioManager::setGroupPaused(Group g, bool paused) {
    if (auto* grp = groupHandle(g)) grp->setPaused(paused);
}

void AudioManager::setListener(const vec3& pos, const vec3& vel, const vec3& forward, const vec3& up) {
    if (!m_system) return;
    FMOD_VECTOR p{ pos.x, pos.y, pos.z };
    FMOD_VECTOR v{ vel.x, vel.y, vel.z };
    FMOD_VECTOR f{ forward.x, forward.y, forward.z };
    FMOD_VECTOR u{ up.x, up.y, up.z };
    m_system->set3DListenerAttributes(0, &p, &v, &f, &u);
}
