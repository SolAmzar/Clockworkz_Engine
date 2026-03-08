# game/CMakeLists.txt — project dispatcher
#
# Auto-discovers any subdirectory under projects/ that has a CMakeLists.txt
# and adds it as a build target. Set ACTIVE_PROJECT to build only one project.
#
# Note: add_custom_command(TARGET) must be called in the same directory the
# target is created. Asset/FMOD copy commands live in each project's own
# CMakeLists.txt. GAME_SHARED_ASSETS_DIR is passed so projects can reference
# the shared game/assets/ (shaders, fonts) without a hardcoded relative path.
 
set(ACTIVE_PROJECT "" CACHE STRING
  "Build only this project subfolder under game/projects/ (empty = build all)")

# Path to shared engine assets (shaders, fonts) — consumed by each project's CMakeLists.txt
set(GAME_SHARED_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")

# --- Discover and add project subdirectories ---
file(GLOB PROJECT_DIRS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/projects"
     "${CMAKE_CURRENT_SOURCE_DIR}/projects/*")

foreach(proj ${PROJECT_DIRS})
  set(proj_dir "${CMAKE_CURRENT_SOURCE_DIR}/projects/${proj}")
  if(IS_DIRECTORY "${proj_dir}" AND EXISTS "${proj_dir}/CMakeLists.txt")
    if("${ACTIVE_PROJECT}" STREQUAL "" OR "${ACTIVE_PROJECT}" STREQUAL "${proj}")
      add_subdirectory("projects/${proj}")
    endif()
  endif()
endforeach()
