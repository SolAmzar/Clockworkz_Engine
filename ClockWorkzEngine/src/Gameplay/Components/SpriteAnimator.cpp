/******************************************************************************/
/*!
\file           SpriteAnimator.cpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Handles 2D sprite animation playback, including updating frame timing,
                switching animation clips, managing loop behavior, and syncing UV data
                with the SpriteRenderer. Supports loading and saving animation clips
                from JSON and provides full control over animation state.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/


#include "Gameplay/Components/SpriteAnimator.hpp"
#include "Gameplay/Components/SpriteRenderer.hpp"
#include "Gameplay/GameObject.hpp"
#include <iostream>

namespace Gameplay {

void SpriteAnimatorComponent::onUpdate(GameObject& owner, double dt) {
    if (!m_playing || m_paused) return;
    if (m_clips.empty() || m_currentClip.empty()) return;

    auto& clip = m_clips[m_currentClip];
    if (clip.frames.empty()) return;

    m_timer += static_cast<float>(dt);
    if (m_timer >= clip.frames[m_frameIndex].duration) {
        m_timer = 0.f;
        m_frameIndex++;

        if (m_frameIndex >= clip.frames.size()) {
            if (clip.loop)
                m_frameIndex = 0;
            else {
                m_frameIndex = clip.frames.size() - 1;
                m_playing = false;
            }
        }
    }

    if (m_renderer) {
        m_renderer->setUV(clip.frames[m_frameIndex].uv);
    }
    else {
        SpriteRendererComponent* renderComp = owner.getComponent<SpriteRendererComponent>();
        if (renderComp) {
            m_renderer = renderComp;
        }
    }
}

void SpriteAnimatorComponent::play(const std::string& name, bool forceRestart) {
    // if same animation, no need to replay same animation.
    if (!forceRestart && m_currentClip == name && m_playing) return;

    auto it = m_clips.find(name);
    if (it == m_clips.end()) {
        std::cerr << "[SpriteAnimator] No animation: " << name << "\n";
        return;
    }

    m_currentClip = name;
    m_frameIndex = 0;
    m_timer = 0.f;
    m_playing = true;

    if (m_renderer && !it->second.frames.empty())
        m_renderer->setUV(it->second.frames[0].uv);
}

void SpriteAnimatorComponent::loadFromJson(const nlohmann::json& j) {
    clearAll();
    m_currentClip = "";
    if (!j.contains("animations")) return;
    m_fileName = j["filename"];
    for (const auto& anim : j["animations"]) {
        AnimationClip clip;
        clip.name = anim["name"].get<std::string>();
        clip.loop = anim.value("loop", true);

        for (const auto& f : anim["frames"]) {
            FrameData fd;
            fd.uv = glm::vec4(f["u"], f["v"], f["w"], f["h"]);
            fd.duration = f.value("duration", 0.1f);
            clip.frames.push_back(fd);
        }
        m_clips[clip.name] = clip;
    }
}

nlohmann::json SpriteAnimatorComponent::serialize(std::string filename) {
    nlohmann::json j;
    j["filename"] = m_fileName;
    for (const auto& [name, clip] : m_clips) {
        nlohmann::json clipJ;
        clipJ["name"] = name;
        
        clipJ["loop"] = clip.loop;
        for (const auto& f : clip.frames) {
            nlohmann::json frame;
            frame["u"] = f.uv.x;
            frame["v"] = f.uv.y;
            frame["w"] = f.uv.z;
            frame["h"] = f.uv.w;
            frame["duration"] = f.duration;
            clipJ["frames"].push_back(frame);
        }
        j["animations"].push_back(clipJ);
    }
    m_fileName = filename;
    return j;
}

bool SpriteAnimatorComponent::hasClip(const std::string& name) const {
    return m_clips.find(name) != m_clips.end();
}

void SpriteAnimatorComponent::addClip(const AnimationClip& clip) {
    m_clips[clip.name] = clip;
}

void SpriteAnimatorComponent::clearAll() {
    m_clips.clear();
    m_currentClip.clear();
    m_playing = false;
    m_frameIndex = 0;
    m_timer = 0.0f;
}

AnimationClip* SpriteAnimatorComponent::getClip(const std::string& name) {
    auto it = m_clips.find(name);
    return (it != m_clips.end()) ? &it->second : nullptr;
}

AnimationClip& SpriteAnimatorComponent::createClip(const std::string& name, bool loop) {
    AnimationClip& clip = m_clips[name];
    clip.name = name;
    clip.loop = loop;
    return clip;
}

bool SpriteAnimatorComponent::addFrameToClip(const std::string& clipName, const glm::vec4& uv, float duration) {
    auto it = m_clips.find(clipName);
    if (it == m_clips.end()) return false;
    it->second.frames.push_back({ uv, duration });
    return true;
}

void SpriteAnimatorComponent::removeClip(const std::string& name) {
    auto it = m_clips.find(name);
    if (it != m_clips.end()) {
        m_clips.erase(it);
        if (m_currentClip == name) {
            m_currentClip.clear();
            m_playing = false;
            m_frameIndex = 0;
            m_timer = 0.0f;
        }
    }
}


} // namespace Gameplay
