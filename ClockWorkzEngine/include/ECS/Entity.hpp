/******************************************************************************/
/*!
\file           Entity.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          Entity definition for ECS architecture.

                Entity is a lightweight handle consisting of:
                - ID: Unique identifier (reused after destruction)
                - Generation: Counter to invalidate old references

                This allows safe entity references even after
destruction/recreation.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include <cstdint>

namespace ECS {

/**
 * @brief Lightweight entity handle
 *
 * An entity is just an ID with a generation counter.
 * When an entity is destroyed, its ID is reused but the generation increments,
 * making old Entity handles invalid.
 */
struct Entity {
  uint32_t id{0};
  uint32_t generation{0};

  bool operator==(const Entity &other) const {
    return id == other.id && generation == other.generation;
  }

  bool operator!=(const Entity &other) const { return !(*this == other); }

  bool isValid() const { return id != 0; }
};

} // namespace ECS
