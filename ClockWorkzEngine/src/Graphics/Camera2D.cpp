/******************************************************************************/
/*!
\file           Camera2D.cpp
\project        Lights Out
\author(s)      [Lebon], 100% - To expand further later like target chasing or
free camera for editor mode.
\brief          Declares Camera2D, a simple orthographic camera for 2D
rendering. Responsibilities include:
                - setOrtho() to configure projection based on window width,
                  height, and scale
                - Maintaining a combined view-projection matrix for rendering
                - Supporting transformations suitable for top-down or
                  side-scrolling gameplay

                Provides a central way to control how the world is viewed
                in screen space.

All content � 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Graphics/Camera2D.hpp"
#include "Audio/AudioManager.hpp" // Added
#include "Core/Engine.hpp"
#include "Math/Vec3.hpp" // Added
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

using namespace Graphics;

void Camera2D::init(int width, int height) {

  setViewport(width, height);
  setPosition(glm::vec2{0, 0});
  setZoom(1.0f);

  Core::Engine::Get().events().subscribe<Core::WindowResizeEvent>(
      this, [this](const Core::WindowResizeEvent &e) {
        this->setViewport(e.width, e.height);
      });
}

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

void Camera2D::setViewport(int width, int height) {
  m_viewW = width;
  m_viewH = height;
  recalcProj();
}
void Camera2D::setPosition(glm::vec2 p) {
  m_pos = p;
  recalcView();
  // Update audio listener to match camera position
  Audio::AudioManager::get().setListener(
      vec3(m_pos.x, m_pos.y, 0.0f), vec3(0.0f, 0.0f, 0.0f),
      vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 1.0f, 0.0f));
}
void Camera2D::setZoom(float z) {
  std::cout << "Zoom: " << z << std::endl;
  m_zoom = z;
  recalcProj();
}
glm::vec2 Camera2D::position() const { return m_pos; }
float Camera2D::zoom() const { return m_zoom; }

const glm::mat4 &Camera2D::viewProj() const { return m_vp; }