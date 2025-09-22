#pragma once
#include <glm/mat4x4.hpp>
namespace Graphics {
	class Camera2D {
	public:
		void setOrtho(float width, float height, float scale = 1.0f);
		const glm::mat4& viewProj() const { return m_vp; }
	private:
		glm::mat4 m_vp{ 1.0f };
	};
}
