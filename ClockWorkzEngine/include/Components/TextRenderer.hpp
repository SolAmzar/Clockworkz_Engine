/******************************************************************************/
/*!
\file           TextRenderer.hpp
\project        ClockWorkz Engine
\brief          TextRenderer component - pure data for text rendering.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <string>

namespace Components {

	enum class TextAlignment { Left = 0, Center, Right };

	struct TextRenderer {
		  std::string text{};                       
		  std::string fontId{"Debug"};
		  float fontSize{1.0f}; 
		  glm::vec3 color{1.0f, 1.0f, 1.0f}; 
		  bool visible{true};
		  glm::vec2 boxSize{200.0f, 100.0f};
		  TextAlignment alignment{TextAlignment::Left};
	};

} // namespace Components
