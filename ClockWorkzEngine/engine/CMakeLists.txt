# engine/CMakeLists.txt � CWZEngine static library
#
# All engine sources live in engine/src/ and engine/include/.
# Game-specific components (EnemyAI, Battery, etc.) live in each game
# project's own include/ folder and are NOT part of the engine library.

# --- Sources & Headers ---
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)
file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

# Ensure LuaBindings.cpp is included (workaround if glob didn't pick it up due to casing/path issues)
set(_lua_bindings_path "${CMAKE_CURRENT_SOURCE_DIR}/src/Scripting/LuaBindings.cpp")
if(EXISTS "${_lua_bindings_path}")
  list(APPEND ENGINE_SOURCES "${_lua_bindings_path}")
  message(STATUS "Forcing inclusion of engine source: ${_lua_bindings_path}")
endif()

# --- Static library target ---
add_library(CWZEngine STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})

# Embed CMake build/source dirs and current config so EditorHub can
# trigger a cmake reconfigure + build at runtime after creating new projects.
target_compile_definitions(CWZEngine PRIVATE
    CWZ_CMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    CWZ_CMAKE_BINARY_DIR="${CMAKE_BINARY_DIR}"
    CWZ_BUILD_CONFIG="$<CONFIG>"
)

# --- Lua / Sol2 ---
import_lua(CWZEngine)

# --- Optional FMOD ---
if(ENGINE_ENABLE_FMOD)
  import_fmod(CWZEngine)
  # Promote DLL paths to CACHE so game/CMakeLists.txt can read them
  # (macro variables don't cross subdirectory boundaries)
  set(FMOD_DLL_DEBUG   "${FMOD_DLL_DEBUG}"   CACHE INTERNAL "FMOD debug DLL path"   FORCE)
  set(FMOD_DLL_RELEASE "${FMOD_DLL_RELEASE}" CACHE INTERNAL "FMOD release DLL path" FORCE)
endif()

# --- FFmpeg ---
import_ffmpeg(CWZEngine)

# --- MSVC compile options ---
if(MSVC)
  target_compile_options(CWZEngine PRIVATE
    /W4 /permissive- /Zc:preprocessor /Zc:__cplusplus /bigobj
    /external:anglebrackets /external:W0
  )
  target_compile_options(CWZEngine PRIVATE
    $<$<CONFIG:Debug>:/Od>
  )
  # PUBLIC so consumers (game) also get these definitions
  target_compile_definitions(CWZEngine PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# --- Non-MSVC warnings ---
if(NOT MSVC)
  target_compile_options(CWZEngine PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Include directories ---
# SYSTEM PUBLIC: game inherits these; SYSTEM suppresses 3rd-party warnings
# LightsOut include is currently needed because some engine/runtime files and
# editor UI still reference project-specific components.
target_include_directories(CWZEngine
  SYSTEM PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# --- Link libraries (PUBLIC so the game executable inherits them) ---
if(WIN32)
  target_link_libraries(CWZEngine PUBLIC
    glfw libglew_static glm::glm freetype
    opengl32 user32 gdi32 shell32
  )
elseif(APPLE)
  target_link_libraries(CWZEngine PUBLIC
    glfw libglew_static glm::glm freetype
    "-framework OpenGL"
  )
else()
  target_link_libraries(CWZEngine PUBLIC
    glfw libglew_static glm::glm freetype GL
  )
endif()

# SDL3 (static) — cross-platform, linked after the platform block
target_link_libraries(CWZEngine PUBLIC SDL3::SDL3-static)
target_include_directories(CWZEngine SYSTEM PUBLIC
  "${CMAKE_BINARY_DIR}/_deps/sdl3-src/include"
)

# --- Source groups for Visual Studio IDE ---
# Split file lists by their root so each source_group gets only files under its TREE root.
set(_ENGINE_HEADERS_LOCAL  "")
set(_ENGINE_HEADERS_TOPLVL "")
foreach(_f IN LISTS ENGINE_HEADERS)
  string(FIND "${_f}" "${CMAKE_CURRENT_SOURCE_DIR}/" _pos)
  if(_pos EQUAL 0)
    list(APPEND _ENGINE_HEADERS_LOCAL  "${_f}")
  else()
    list(APPEND _ENGINE_HEADERS_TOPLVL "${_f}")
  endif()
endforeach()

set(_ENGINE_SOURCES_LOCAL  "")
set(_ENGINE_SOURCES_TOPLVL "")
foreach(_f IN LISTS ENGINE_SOURCES)
  string(FIND "${_f}" "${CMAKE_CURRENT_SOURCE_DIR}/" _pos)
  if(_pos EQUAL 0)
    list(APPEND _ENGINE_SOURCES_LOCAL  "${_f}")
  else()
    list(APPEND _ENGINE_SOURCES_TOPLVL "${_f}")
  endif()
endforeach()

if(_ENGINE_HEADERS_LOCAL)
  source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" PREFIX "Engine/Headers" FILES ${_ENGINE_HEADERS_LOCAL})
endif()
if(_ENGINE_HEADERS_TOPLVL)
  source_group(TREE "${CMAKE_SOURCE_DIR}/include" PREFIX "Top/Headers" FILES ${_ENGINE_HEADERS_TOPLVL})
endif()
if(_ENGINE_SOURCES_LOCAL)
  source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Engine/Sources" FILES ${_ENGINE_SOURCES_LOCAL})
endif()
if(_ENGINE_SOURCES_TOPLVL)
  source_group(TREE "${CMAKE_SOURCE_DIR}/src" PREFIX "Top/Sources" FILES ${_ENGINE_SOURCES_TOPLVL})
endif()
