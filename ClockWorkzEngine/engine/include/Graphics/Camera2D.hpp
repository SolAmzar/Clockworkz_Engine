/******************************************************************************/
/*!
\file           Camera2D.hpp
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

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

namespace Graphics {
  class Camera2D {
  public:
      void init(int width, int height);
      void setViewport(int width, int height);
      void setPosition(glm::vec2 p);
      void setZoom(float z); 
      glm::vec2 position() const; 
      float zoom() const; 
      const glm::mat4& viewProj() const; 

  private:
    void recalcProj();
    void recalcView();
    void recalcVP();

    int m_viewW = 800, m_viewH = 600;
    glm::vec2 m_pos{0.0f, 0.0f};
    float m_zoom = 1.0f;

    glm::mat4 m_proj{1.0f};
    glm::mat4 m_view{1.0f};
    glm::mat4 m_vp{1.0f};
  };
}

