/******************************************************************************/
/*!
\file           Camera2D.cpp
\project        Lights Out
\author(s)      [Lebon], 100% - To expand further later like target chasing or free camera for editor mode.
\brief          Declares Camera2D, a simple orthographic camera for 2D rendering.
                Responsibilities include:
                - setOrtho() to configure projection based on window width,
                  height, and scale
                - Maintaining a combined view-projection matrix for rendering
                - Supporting transformations suitable for top-down or
                  side-scrolling gameplay

                Provides a central way to control how the world is viewed
                in screen space.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Graphics/Camera2D.hpp"
#include <glm/gtc/matrix_transform.hpp>

using namespace Graphics;

void Camera2D::setOrtho(float width, float height, float scale) {
	float hw = (width * 0.5f) / scale;
	float hh = (height * 0.5f) / scale;
	// TODO double check can use glm or have to use own matrix4x4
	glm::mat4 proj = glm::ortho(-hw, hw, -hh, hh, -1.0f, 1.0f);
	glm::mat4 view = glm::mat4(1.0f);
	m_vp = proj * view;
}
