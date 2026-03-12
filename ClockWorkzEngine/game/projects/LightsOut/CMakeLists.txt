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
# Helper macro: compile/link settings only (NO asset copies — kept out of the
# macro to prevent parallel-build races when ninja builds both targets at once)
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
    VS_DEBUGGER_WORKING_DIRECTORY    "$<TARGET_FILE_DIR:${TARGET_NAME}>"
  )
endmacro()

# ---------------------------------------------------------------------------
# FMOD copies — single shared custom target to prevent parallel-build races.
# Both executables output to the same bin/<Config>/ directory, so concurrent
# POST_BUILD fmod copies would collide when ninja builds them simultaneously.
# ---------------------------------------------------------------------------
set(_fmod_dir "${CMAKE_SOURCE_DIR}/fmod")
if(EXISTS "${_fmod_dir}")
  add_custom_target(CopyFmod
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${_fmod_dir}"
      "${CMAKE_BINARY_DIR}/fmod"
    COMMENT "Copying FMOD directory"
    VERBATIM
  )
  add_dependencies(CWZEditor CopyFmod)
  add_dependencies(LightsOut CopyFmod)
endif()

if(ENGINE_ENABLE_FMOD AND WIN32)
  add_custom_target(CopyFmodDll
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<IF:$<CONFIG:Debug>,${FMOD_DLL_DEBUG},${FMOD_DLL_RELEASE}>"
      "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/"
    COMMENT "Copying FMOD runtime DLL"
    VERBATIM
  )
  add_dependencies(CWZEditor CopyFmodDll)
  add_dependencies(LightsOut CopyFmodDll)
endif()

if(WIN32 AND FFMPEG_DLLS)
  add_custom_target(CopyFFmpegDll
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${FFMPEG_DLLS}
      "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/"
    COMMENT "Copying FFmpeg runtime DLLs"
    VERBATIM
  )
  add_dependencies(CWZEditor CopyFFmpegDll)
  add_dependencies(LightsOut CopyFFmpegDll)
endif()

configure_lightsout_target(CWZEditor)
configure_lightsout_target(LightsOut)

# ---------------------------------------------------------------------------
# Asset copies — done via a single custom target so they never run in parallel.
# Both executables go to the same bin/Debug (or bin/Release) directory, so
# concurrent copies of the same source trees would race.
# ---------------------------------------------------------------------------

# 1. Shared engine assets (shaders, fonts, engine_config, empty textures stub)
#    → bin/<Config>/assets/
if(DEFINED GAME_SHARED_ASSETS_DIR AND EXISTS "${GAME_SHARED_ASSETS_DIR}")
  add_custom_target(CopySharedAssets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${GAME_SHARED_ASSETS_DIR}"
      "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/assets"
    COMMENT "Copying shared game assets"
    VERBATIM
  )
  add_dependencies(CWZEditor CopySharedAssets)
  add_dependencies(LightsOut CopySharedAssets)
endif()

# 2. LightsOut-specific assets — runs as a custom target that depends on
#    CopySharedAssets, so game assets (Textures.json, scenes/, textures/, etc.)
#    always overwrite the empty shared stubs regardless of which executable was
#    built last.  Both CWZEditor and LightsOut depend on this target so the
#    correct assets are always present in the shared bin/<Config>/ directory.
add_custom_target(CopyLightsOutAssets
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/projects/LightsOut"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/assets"
  # Mirror shared engine fonts into the project dir so the editor can resolve
  # "assets/fonts/MANIC1.ttf" relative to projects/LightsOut/ without absolute paths.
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${GAME_SHARED_ASSETS_DIR}/fonts"
    "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/projects/LightsOut/assets/fonts"
  COMMENT "Copying LightsOut game assets"
  VERBATIM
)
if(TARGET CopySharedAssets)
  add_dependencies(CopyLightsOutAssets CopySharedAssets)
endif()
add_dependencies(CWZEditor CopyLightsOutAssets)
add_dependencies(LightsOut CopyLightsOutAssets)

# --- VS source groups ---
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"     PREFIX "Source Files" FILES ${ALL_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" PREFIX "Header Files" FILES ${HEADERS})
