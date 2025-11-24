# Maintained by Huang Po-Han

cmake_minimum_required(VERSION 3.24)
project(ClockWorkzEngine VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# --- C++ standard ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Import dependencies ---
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(ImportDependencies)
importDependencies()  # fetch/build GLFW, GLEW, GLM, FreeType, ImGui


# ----Compiling using GLEW (For IMGUI) ----
add_compile_definitions(IMGUI_IMPL_OPENGL_LOADER_CUSTOM)

# --- Sources & Headers ---
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_SOURCE_DIR}/src/*.c"
)
file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/include/*.h"
  "${CMAKE_SOURCE_DIR}/include/*.hpp"
)

# Fallback if empty (lets you configure before writing code)
if(ENGINE_SOURCES STREQUAL "" AND ENGINE_HEADERS STREQUAL "")
  file(WRITE "${CMAKE_BINARY_DIR}/_dummy.cpp" "int main(){return 0;}\n")
  set(ENGINE_SOURCES "${CMAKE_BINARY_DIR}/_dummy.cpp" "src/Graphics/IMGUI.cpp" "src/Graphics/ImGuiManager.cpp")
endif()

add_executable(${PROJECT_NAME} ${ENGINE_SOURCES} ${ENGINE_HEADERS}  "src/Graphics/ImGuiManager.cpp" "src/Platform/LevelEditor.cpp" "src/Platform/LevelEditorState.cpp")

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /Zc:preprocessor /Zc:__cplusplus)
endif()

# --- Optionally import FMOD ---
if(ENGINE_ENABLE_FMOD)
    import_fmod(${PROJECT_NAME})
endif()

# --- Default build type for single-config generators ---
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# --- Warnings & MSVC niceties ---
if(MSVC)
  add_compile_options(/W4 /permissive- /Zc:preprocessor /Zc:__cplusplus)
  add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN)
else()
  add_compile_options(-
  -Wextra -Wpedantic)
endif()

# --- Silence deprecation warnings for 3rd-party libs ---
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

# --- Public/Private include directories ---
target_include_directories(${PROJECT_NAME}
  SYSTEM PUBLIC  ${CMAKE_SOURCE_DIR}/include
  PRIVATE ${CMAKE_SOURCE_DIR}/src
)

# --- Source groups for IDEs like Visual Studio ---
source_group(TREE "${CMAKE_SOURCE_DIR}/include" PREFIX "Header Files" FILES ${ENGINE_HEADERS})
source_group(TREE "${CMAKE_SOURCE_DIR}/src"     PREFIX "Source Files" FILES ${ENGINE_SOURCES})

# --- Link platform-specific libraries + dependencies ---
if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE 
    glfw libglew_static glm::glm freetype
    opengl32 user32 gdi32 shell32
  )
elseif(APPLE)
  target_link_libraries(${PROJECT_NAME} PRIVATE 
    glfw libglew_static glm::glm freetype
    "-framework OpenGL"
  )
else()
  target_link_libraries(${PROJECT_NAME} PRIVATE 
    glfw libglew_static glm::glm freetype GL
  )
endif()

# --- Output folders (multi-config friendly) ---
set_target_properties(${PROJECT_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
)

# --- Copy assets folder once to both locations ---
set(ENGINE_ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets")
if(EXISTS "${ENGINE_ASSETS_DIR}")
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${ENGINE_ASSETS_DIR}"
      "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
  )
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${ENGINE_ASSETS_DIR}"
      "${CMAKE_BINARY_DIR}/assets"
  )
endif()

# --- copy fmod into build/fmod for Visual Studio ---
set(ENGINE_FMOD_DIR "${CMAKE_SOURCE_DIR}/fmod")
if(EXISTS "${ENGINE_FMOD_DIR}")
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${ENGINE_FMOD_DIR}"
      "${CMAKE_BINARY_DIR}/fmod")
endif()

# --- Copy FMOD runtime DLL into build/bin ---
if (ENGINE_ENABLE_FMOD AND WIN32)
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<IF:$<CONFIG:Debug>,${FMOD_DLL_DEBUG},${FMOD_DLL_RELEASE}>"
      "$<TARGET_FILE_DIR:${PROJECT_NAME}>/")
endif()

# Collect asset files
file(GLOB_RECURSE ENGINE_ASSETS CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/assets/*.*"
)

# Add them to the project so Visual Studio shows them
target_sources(${PROJECT_NAME} PRIVATE ${ENGINE_ASSETS})

# Put them in their own filter
source_group(TREE "${CMAKE_SOURCE_DIR}/assets" PREFIX "Assets" FILES ${ENGINE_ASSETS})

# Make CMake aware of asset files so rebuild triggers on changes
set(ENGINE_ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets")
file(GLOB_RECURSE ENGINE_ASSETS CONFIGURE_DEPENDS
  "${ENGINE_ASSETS_DIR}/*.*"
)

# Add a custom target to always copy assets
add_custom_target(copy_assets ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${ENGINE_ASSETS_DIR}"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${ENGINE_ASSETS_DIR}"
    "${CMAKE_BINARY_DIR}/assets"
  DEPENDS ${ENGINE_ASSETS}
)

# Make the exe depend on this
add_dependencies(${PROJECT_NAME} copy_assets)


# --- Default startup project in Visual Studio ---
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})