cmake_minimum_required(VERSION 3.24)
project(ClockWorkzEngine VERSION 0.1.0 LANGUAGES C CXX)

# --- C++ standard ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Default to Release if single-config generator (Ninja/Make) ---
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# --- Warnings & MSVC niceties ---
if(MSVC)
  add_compile_options(/W4 /permissive- /Zc:preprocessor /Zc:__cplusplus)
  add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Optionally silence 3rd-party deprecation spam (e.g., GLM min cmake)
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

# --- Dependencies ---
include(FetchContent)

# GLFW
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

# GLM (header-only)
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

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
  set(ENGINE_SOURCES "${CMAKE_BINARY_DIR}/_dummy.cpp")
endif()

add_executable(${PROJECT_NAME} ${ENGINE_SOURCES} ${ENGINE_HEADERS})

# Public headers in include/, private headers in src/
target_include_directories(${PROJECT_NAME}
  PUBLIC  ${CMAKE_SOURCE_DIR}/include
  PRIVATE ${CMAKE_SOURCE_DIR}/src
)

# Filters in Visual Studio (Header Files / Source Files)
source_group(TREE "${CMAKE_SOURCE_DIR}/include" PREFIX "Header Files" FILES ${ENGINE_HEADERS})
source_group(TREE "${CMAKE_SOURCE_DIR}/src"     PREFIX "Source Files" FILES ${ENGINE_SOURCES})

# --- Link against system OpenGL explicitly + platform bits ---
if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE glfw glm::glm opengl32 user32 gdi32 shell32)
elseif(APPLE)
  target_link_libraries(${PROJECT_NAME} PRIVATE glfw glm::glm "-framework OpenGL")
else()
  target_link_libraries(${PROJECT_NAME} PRIVATE glfw glm::glm GL)
endif()

# --- Output folders tidy (nice for multi-config VS) ---
set_target_properties(${PROJECT_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
)

# --- copy assets/ next to the exe after build ---
set(ENGINE_ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets")
if(EXISTS "${ENGINE_ASSETS_DIR}")
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${ENGINE_ASSETS_DIR}"
      "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets")
endif()


