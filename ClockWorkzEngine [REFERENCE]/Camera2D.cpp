#include "Graphics/Camera2D.hpp"
#include <glm/gtc/matrix_transform.hpp>

using namespace Graphics;

void Camera2D::setOrtho(float width, float height, float scale) {
	float hw = (width * 0.5f) / scale;
	float hh = (height * 0.5f) / scale;
	glm::mat4 proj = glm::ortho(-hw, hw, -hh, hh, -1.0f, 1.0f);
	glm::mat4 view = glm::mat4(1.0f);
	m_vp = proj * view;
}
