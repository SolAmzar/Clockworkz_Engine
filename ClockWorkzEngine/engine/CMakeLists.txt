# engine/CMakeLists.txt — ClockWorkzEngine static library
#
# STEP 2: Pure engine subsystems live in engine/src/ and engine/include/.
# Remaining game-specific and not-yet-moved files still live in top-level src/ and include/.
# Both locations are globbed; game-specific files are split out later (Steps 3-9).

# --- Sources & Headers ---
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_SOURCE_DIR}/src/*.c"
)
file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
  "${CMAKE_SOURCE_DIR}/include/*.h"
  "${CMAKE_SOURCE_DIR}/include/*.hpp"
)

# Exclude main.cpp — it belongs to the game executable
list(FILTER ENGINE_SOURCES EXCLUDE REGEX ".*/src/main\\.cpp$")

# --- Static library target ---
add_library(ClockWorkzEngine STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})

# --- Lua / Sol2 ---
import_lua(ClockWorkzEngine)

# --- Optional FMOD ---
if(ENGINE_ENABLE_FMOD)
  import_fmod(ClockWorkzEngine)
  # Promote DLL paths to CACHE so game/CMakeLists.txt can read them
  # (macro variables don't cross subdirectory boundaries)
  set(FMOD_DLL_DEBUG   "${FMOD_DLL_DEBUG}"   CACHE INTERNAL "FMOD debug DLL path"   FORCE)
  set(FMOD_DLL_RELEASE "${FMOD_DLL_RELEASE}" CACHE INTERNAL "FMOD release DLL path" FORCE)
endif()

# --- MSVC compile options ---
if(MSVC)
  target_compile_options(ClockWorkzEngine PRIVATE
    /W4 /permissive- /Zc:preprocessor /Zc:__cplusplus /bigobj
  )
  target_compile_options(ClockWorkzEngine PRIVATE
    $<$<CONFIG:Debug>:/Od>
  )
  # PUBLIC so consumers (game) also get these definitions
  target_compile_definitions(ClockWorkzEngine PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# --- Non-MSVC warnings ---
if(NOT MSVC)
  target_compile_options(ClockWorkzEngine PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Include directories ---
# SYSTEM PUBLIC: game inherits these; SYSTEM suppresses 3rd-party warnings
target_include_directories(ClockWorkzEngine
  SYSTEM PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/include"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/src"
)

# --- Link libraries (PUBLIC so the game executable inherits them) ---
if(WIN32)
  target_link_libraries(ClockWorkzEngine PUBLIC
    glfw libglew_static glm::glm freetype
    opengl32 user32 gdi32 shell32
  )
elseif(APPLE)
  target_link_libraries(ClockWorkzEngine PUBLIC
    glfw libglew_static glm::glm freetype
    "-framework OpenGL"
  )
else()
  target_link_libraries(ClockWorkzEngine PUBLIC
    glfw libglew_static glm::glm freetype GL
  )
endif()

# --- Source groups for Visual Studio IDE ---
# Split file lists by their root so each source_group gets only files under its TREE root.
set(_ENGINE_HEADERS_LOCAL  "")
set(_ENGINE_HEADERS_TOPLVL "")
foreach(_f IN LISTS ENGINE_HEADERS)
  if(_f MATCHES "^${CMAKE_CURRENT_SOURCE_DIR}/")
    list(APPEND _ENGINE_HEADERS_LOCAL  "${_f}")
  else()
    list(APPEND _ENGINE_HEADERS_TOPLVL "${_f}")
  endif()
endforeach()

set(_ENGINE_SOURCES_LOCAL  "")
set(_ENGINE_SOURCES_TOPLVL "")
foreach(_f IN LISTS ENGINE_SOURCES)
  if(_f MATCHES "^${CMAKE_CURRENT_SOURCE_DIR}/")
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
