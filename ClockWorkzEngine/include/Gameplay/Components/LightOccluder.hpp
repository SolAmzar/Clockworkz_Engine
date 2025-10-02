/******************************************************************************/
/*!
\file           LightOccluder.hpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares LightOccluder, a component that marks a GameObject
				as blocking or allowing light. It provides methods to enable
				or disable occlusion dynamically, allowing integration with
				lighting and shadow systems.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

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
