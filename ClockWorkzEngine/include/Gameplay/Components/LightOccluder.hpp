#pragma once
#include "Gameplay/GameObject.hpp"

namespace Gameplay {
	class LightOccluder final : public Component {
	public:
		explicit LightOccluder(bool enabled = true) : m_enabled(enabled) {}
		bool enabled() const { return m_enabled; }
		void setEnabled(bool v) { m_enabled = v; }
	private:
		bool m_enabled{ true };
	};
}
