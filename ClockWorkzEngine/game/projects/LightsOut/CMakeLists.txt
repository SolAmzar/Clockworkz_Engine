# game/projects/LightsOut/CMakeLists.txt
#
# Produces two executables from the same shared source tree:
#   CWZEditor  — Editor tool (boots to EditorHubState)   entry: src/main.cpp
#   LightsOut  — Game        (boots to MenuState)         entry: src/main_game.cpp
#
# Note: the engine static library is also named CWZEngine, so the editor
# executable must use a different name (CWZEditor) to avoid a CMake conflict.
#
# GAME_SHARED_ASSETS_DIR is set by the parent game/CMakeLists.txt dispatcher.

file(GLOB_RECURSE ALL_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE HEADERS     CONFIGURE_DEPENDS "include/*.hpp")

# Shared sources: everything EXCEPT the two entry-point mains
set(SHARED_SOURCES ${ALL_SOURCES})
list(FILTER SHARED_SOURCES EXCLUDE REGEX ".*[/\\]main[^/\\]*\\.cpp$")

# ---- Editor executable ----
add_executable(CWZEditor ${SHARED_SOURCES} ${HEADERS} src/main.cpp)

# ---- Game executable ----
add_executable(LightsOut ${SHARED_SOURCES} ${HEADERS} src/main_game.cpp)

# ---------------------------------------------------------------------------
# Helper macro: apply identical compile/link/asset settings to both targets
# ---------------------------------------------------------------------------
macro(configure_lightsout_target TARGET_NAME)
  target_include_directories(${TARGET_NAME} PRIVATE include src)
  target_link_libraries(${TARGET_NAME} PRIVATE CWZEngine)

  if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE
      /W4 /permissive- /Zc:preprocessor /Zc:__cplusplus /bigobj
      $<$<CONFIG:Debug>:/Od>
    )
    target_link_options(${TARGET_NAME} PRIVATE
      $<$<CONFIG:Debug>:/INCREMENTAL>
    )
  endif()

  set_target_properties(${TARGET_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
  )

  # Copy this project's runtime data (scenes, scripts, textures, audio)
  add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/assets"
      "$<TARGET_FILE_DIR:${TARGET_NAME}>/projects/LightsOut"
  )

  # Copy shared engine assets (shaders, fonts) from game/assets/
  if(DEFINED GAME_SHARED_ASSETS_DIR AND EXISTS "${GAME_SHARED_ASSETS_DIR}")
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${GAME_SHARED_ASSETS_DIR}"
        "$<TARGET_FILE_DIR:${TARGET_NAME}>/assets"
    )
  endif()

  # Copy FMOD directory
  set(_fmod_dir "${CMAKE_SOURCE_DIR}/fmod")
  if(EXISTS "${_fmod_dir}")
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${_fmod_dir}"
        "${CMAKE_BINARY_DIR}/fmod"
    )
  endif()

  # Copy FMOD runtime DLL
  if(ENGINE_ENABLE_FMOD AND WIN32)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<IF:$<CONFIG:Debug>,${FMOD_DLL_DEBUG},${FMOD_DLL_RELEASE}>"
        "$<TARGET_FILE_DIR:${TARGET_NAME}>/"
    )
  endif()
endmacro()

configure_lightsout_target(CWZEditor)
configure_lightsout_target(LightsOut)

# --- VS source groups ---
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"     PREFIX "Source Files" FILES ${ALL_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" PREFIX "Header Files" FILES ${HEADERS})
