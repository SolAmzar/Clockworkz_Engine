/******************************************************************************/
/*!
\file           TextRenderer.hpp
\project        ClockWorkz Engine
\brief          TextRenderer component - pure data for text rendering.
                Similar to Unity's Text component: position = pivot anchor,
                boxSize defines the text area, alignment controls layout.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "Math/Vec2.hpp"
#include <glm/vec3.hpp>
#include <string>

namespace Components {

enum class TextAlignment    { Left = 0, Center, Right };
enum class VerticalAlignment{ Top  = 0, Middle, Bottom };

struct TextRenderer {
    std::string      text{};
    std::string      fontId{"Debug"};
    float            fontSize{1.0f};
    glm::vec3        color{1.0f, 1.0f, 1.0f};
    bool             visible{true};

    // Box that defines the text area
    vec2             boxSize{200.0f, 100.0f};

    // Horizontal alignment drives the gizmo anchor:
    //   Left   → gizmo = left edge  (text extends right)
    //   Center → gizmo = center     (text extends equally left/right)
    //   Right  → gizmo = right edge (text extends left)
    // Vertical: gizmo = top of text block (text flows downward)
    TextAlignment    alignment     {TextAlignment::Left};      // horizontal
    VerticalAlignment verticalAlign{VerticalAlignment::Top};   // vertical within box

    bool             wordWrap    {true};   // wrap text to box width
    float            lineSpacing {1.0f};   // multiplier on line height

    // Inner padding (pixels): x = left/right, y = top/bottom
    vec2             padding     {0.0f, 0.0f};
};

} // namespace Components
