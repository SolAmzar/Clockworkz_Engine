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

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

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
