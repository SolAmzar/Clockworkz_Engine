#pragma once
#include <string>
#include "Gameplay/Surface.hpp"
#include "Gameplay/GameObject.hpp"


namespace Gameplay {

	class SurfaceComponent final : public Component {

	public:
		explicit SurfaceComponent(SurfaceType t =  SurfaceType::Default)
			: m_type(t) {}
		SurfaceType type() const { return m_type;  }
		void setType(SurfaceType t) { m_type = t; }

	private:
		SurfaceType m_type{ SurfaceType::Default };
	};
} // namespace Gameplay

