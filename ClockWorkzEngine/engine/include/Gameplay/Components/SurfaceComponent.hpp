/******************************************************************************/
/*!
\file			SurfaceComponent.hpp
\project        Lights Out
\author(s)      [Sol-Amzar], 100%
\brief          Declares SurfaceComponent to associate surface types with
GameObjects. Responsibilities include:
                                - Storing and retrieving the SurfaceType of the
GameObject
                                - Allowing dynamic changes to surface type at
runtime All content ? 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "Gameplay/Surface.hpp"
#include <string>


namespace Gameplay {

class SurfaceComponent final {

public:
  explicit SurfaceComponent(SurfaceType t = SurfaceType::Default) : m_type(t) {}
  SurfaceType type() const { return m_type; }
  void setType(SurfaceType t) { m_type = t; }

private:
  SurfaceType m_type{SurfaceType::Default};
};
} // namespace Gameplay
