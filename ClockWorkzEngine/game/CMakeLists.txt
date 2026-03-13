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

# ---------------------------------------------------------------------------
# Global Shared Copy Targets
# ---------------------------------------------------------------------------
# These are defined once here to prevent duplicate-target errors and parallel
# build races when multiple projects are built into the same shared bin/ folder.

# 1. FMOD directory
set(_fmod_dir "${CMAKE_SOURCE_DIR}/fmod")
if(EXISTS "${_fmod_dir}" AND NOT TARGET CopyFmod)
  add_custom_target(CopyFmod
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${_fmod_dir}" "${CMAKE_BINARY_DIR}/fmod"
    COMMENT "Copying FMOD directory" VERBATIM)
endif()

# 2. FMOD Runtime DLL
if(ENGINE_ENABLE_FMOD AND WIN32 AND NOT TARGET CopyFmodDll)
  add_custom_target(CopyFmodDll
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<IF:$<CONFIG:Debug>,${FMOD_DLL_DEBUG},${FMOD_DLL_RELEASE}>"
      "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/"
    COMMENT "Copying FMOD runtime DLL" VERBATIM)
endif()

# 3. Shared engine assets (shaders, fonts, engine_config)
if(EXISTS "${GAME_SHARED_ASSETS_DIR}" AND NOT TARGET CopySharedAssets)
  add_custom_target(CopySharedAssets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${GAME_SHARED_ASSETS_DIR}" "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/assets"
    COMMENT "Copying shared game assets" VERBATIM)
endif()

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
