# Root CMakeLists.txt — ClockWorkz Project
# Orchestrates the engine (static library) and game (executable) sub-projects.

cmake_minimum_required(VERSION 3.24)
project(ClockWorkzProject VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# --- C++ standard ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- MSVC: Replace /Zi with /ZI globally for Edit and Continue support ---
if(MSVC)
  string(REPLACE "/Zi" "/ZI" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  string(REPLACE "/Zi" "/ZI" CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}")
endif()

# --- Default build type for single-config generators ---
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# --- Shared CMake modules ---
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(ImportDependencies)
include(ImportLua)
importDependencies()  # fetch/build GLFW, GLEW, GLM, FreeType, ImGui

# --- Global compile definitions (needed by both engine and game) ---
# GLEW must be included before gl.h — tell ImGui to use a custom loader
add_compile_definitions(IMGUI_IMPL_OPENGL_LOADER_CUSTOM)
# Prevent GLFW from pulling in gl.h directly
add_compile_definitions(GLFW_INCLUDE_NONE)

# --- Silence deprecation warnings for 3rd-party libs ---
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

# --- Sub-projects ---
add_subdirectory(engine)
add_subdirectory(game)

# --- Default startup project in Visual Studio ---
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT LightsOut)
