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

void Camera2D::recalcProj() {
  const float halfW = (m_viewW * 0.5f) / m_zoom;
  const float halfH = (m_viewH * 0.5f) / m_zoom;
  m_proj = glm::ortho(-halfW, halfW, -halfH, halfH, -1.0f, 1.0f);
  recalcVP();
}
void Camera2D::recalcView() {
  m_view = glm::translate(glm::mat4(1.0f), {-m_pos.x, -m_pos.y, 0.0f});
  recalcVP();
}
void Camera2D::recalcVP() { m_vp = m_proj * m_view; }
