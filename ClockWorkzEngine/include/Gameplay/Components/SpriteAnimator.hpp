#pragma once
#include "Core/json.hpp"
#include "Gameplay/GameObject.hpp"
#include "Gameplay/Components/SpriteRenderer.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <glm/vec4.hpp>

namespace Gameplay {

    struct FrameData {
        glm::vec4 uv;
        float duration;
    };

    struct AnimationClip {
        std::string name;
        std::vector<FrameData> frames;
        bool loop = true;
    };

    class SpriteAnimatorComponent : public Component {
    public:
        SpriteAnimatorComponent() = default;

        // --- Core Update ---
        void onUpdate(GameObject& owner, double dt) override;
        void play(const std::string& name, bool forceRestart = false);
        void stop() { m_playing = false; }
        void setPaused(bool p) { m_paused = p; }

        // --- JSON (load/save) ---
        void loadFromJson(const nlohmann::json& j);
        nlohmann::json serialize() const;

        // --- Animation Clip Management ---
        bool hasClip(const std::string& name) const;
        void addClip(const AnimationClip& clip);
        AnimationClip* getClip(const std::string& name);
        AnimationClip& createClip(const std::string& name, bool loop = true);

        const std::unordered_map<std::string, AnimationClip>& clips() const { return m_clips; }
        std::unordered_map<std::string, AnimationClip>& clips() { return m_clips; }

        // Add a single frame to a specific clip (if exists)
        bool addFrameToClip(const std::string& clipName, const glm::vec4& uv, float duration);

        std::string currentState() const { return m_currentClip; }
        void setSpriteRenderer(SpriteRendererComponent* sr) { m_renderer = sr; }

        void removeClip(const std::string& name);

    private:
        std::unordered_map<std::string, AnimationClip> m_clips;
        std::string m_currentClip;
        float m_timer = 0.f;
        size_t m_frameIndex = 0;
        bool m_playing = false;
        bool m_paused = false;
        SpriteRendererComponent* m_renderer = nullptr;
    };

} // namespace Gameplay
