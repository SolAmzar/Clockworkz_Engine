# game/projects/LightsOut/CMakeLists.txt — LightsOut game executable
#
# Self-contained build: globs its own src/ and include/, links ClockWorkzEngine.
# GAME_SHARED_ASSETS_DIR is set by the parent game/CMakeLists.txt dispatcher and
# points to game/assets/ (shared shaders, fonts) which this target copies to output.

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "include/*.hpp")

add_executable(LightsOut ${SOURCES} ${HEADERS})

target_include_directories(LightsOut PRIVATE include src)
target_link_libraries(LightsOut PRIVATE ClockWorkzEngine)

# --- MSVC compile options ---
if(MSVC)
  target_compile_options(LightsOut PRIVATE
    /W4 /permissive- /Zc:preprocessor /Zc:__cplusplus /bigobj
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

# --- Copy this project's runtime data (scenes, scripts, textures, audio) ---
add_custom_command(TARGET LightsOut POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:LightsOut>/projects/LightsOut"
)

# --- Copy shared engine assets (shaders, fonts) from game/assets/ ---
if(DEFINED GAME_SHARED_ASSETS_DIR AND EXISTS "${GAME_SHARED_ASSETS_DIR}")
  add_custom_command(TARGET LightsOut POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${GAME_SHARED_ASSETS_DIR}"
      "$<TARGET_FILE_DIR:LightsOut>/assets"
  )
endif()

# --- Copy FMOD directory ---
set(_fmod_dir "${CMAKE_SOURCE_DIR}/fmod")
if(EXISTS "${_fmod_dir}")
  add_custom_command(TARGET LightsOut POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${_fmod_dir}"
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

# --- VS source groups ---
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"     PREFIX "Source Files" FILES ${SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" PREFIX "Header Files" FILES ${HEADERS})
