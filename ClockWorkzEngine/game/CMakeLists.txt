# game/CMakeLists.txt — LightsOut game executable
#
# Links against the ClockWorkzEngine static library.
# Game-specific sources live in game/src/ and game/include/.
# Assets live in game/assets/ (currently still at top-level assets/ — see fallback below).

# --- Game Sources & Headers ---
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
file(GLOB_RECURSE GAME_HEADERS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

# --- Executable target ---
add_executable(LightsOut ${GAME_SOURCES} ${GAME_HEADERS})

# --- Include directories ---
# Engine's PUBLIC includes (engine/include, top-level include) are inherited via
# target_link_libraries. Add game-local include roots here.
target_include_directories(LightsOut PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# --- Link against engine (inherits engine's PUBLIC includes + libraries) ---
target_link_libraries(LightsOut PRIVATE ClockWorkzEngine)

# --- MSVC compile options ---
if(MSVC)
  target_compile_options(LightsOut PRIVATE
    /W4 /permissive- /Zc:preprocessor /Zc:__cplusplus /bigobj
  )
  target_compile_options(LightsOut PRIVATE
    $<$<CONFIG:Debug>:/Od>
  )
  target_link_options(LightsOut PRIVATE
    $<$<CONFIG:Debug>:/INCREMENTAL>
  )
endif()

# --- Output directory ---
set_target_properties(LightsOut PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_BINARY_DIR}/bin"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/bin/Debug"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
)

# --- Copy assets to output directory ---
# Uses game/assets/ once it exists; falls back to top-level assets/ during transition.
set(GAME_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
if(EXISTS "${GAME_ASSETS_DIR}")
  set(ACTIVE_ASSETS_DIR "${GAME_ASSETS_DIR}")
else()
  set(ACTIVE_ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets")
endif()

if(EXISTS "${ACTIVE_ASSETS_DIR}")
  add_custom_command(TARGET LightsOut POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${ACTIVE_ASSETS_DIR}"
      "$<TARGET_FILE_DIR:LightsOut>/assets"
  )
  add_custom_command(TARGET LightsOut POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${ACTIVE_ASSETS_DIR}"
      "${CMAKE_BINARY_DIR}/assets"
  )
endif()

# --- Copy projects to output directory ---
# Each project lives in game/projects/<Name>/ and must be available at runtime
# for the editor to read project-specific assets directly from source.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/projects")
  add_custom_command(TARGET LightsOut POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/projects"
      "$<TARGET_FILE_DIR:LightsOut>/projects"
  )
endif()

# --- Copy FMOD directory into build ---
set(ENGINE_FMOD_DIR "${CMAKE_SOURCE_DIR}/fmod")
if(EXISTS "${ENGINE_FMOD_DIR}")
  add_custom_command(TARGET LightsOut POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${ENGINE_FMOD_DIR}"
      "${CMAKE_BINARY_DIR}/fmod"
  )
endif()

# --- Copy FMOD runtime DLL ---
if(ENGINE_ENABLE_FMOD AND WIN32)
  add_custom_command(TARGET LightsOut POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<IF:$<CONFIG:Debug>,${FMOD_DLL_DEBUG},${FMOD_DLL_RELEASE}>"
      "$<TARGET_FILE_DIR:LightsOut>/"
  )
endif()

# --- Collect game assets for VS project display ---
file(GLOB_RECURSE GAME_ASSETS CONFIGURE_DEPENDS
  "${ACTIVE_ASSETS_DIR}/*.*"
)
target_sources(LightsOut PRIVATE ${GAME_ASSETS})
source_group(TREE "${ACTIVE_ASSETS_DIR}" PREFIX "Assets" FILES ${GAME_ASSETS})

# --- VS source groups ---
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"     PREFIX "Source Files" FILES ${GAME_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" PREFIX "Header Files" FILES ${GAME_HEADERS})
