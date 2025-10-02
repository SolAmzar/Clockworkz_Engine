cmake_minimum_required(VERSION 3.24)
project(ClockWorkzEngine VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

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

# --- GLEW ---
set(BUILD_UTILS OFF CACHE BOOL "" FORCE)
set(glew-cmake_BUILD_SHARED OFF CACHE BOOL "" FORCE)  # build static library
FetchContent_Declare(
  glew
  GIT_REPOSITORY https://github.com/Perlmint/glew-cmake.git
  GIT_TAG glew-cmake-2.2.0
)
FetchContent_MakeAvailable(glew)

# GLM (header-only)
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)


# FreeType
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "Disable HarfBuzz" FORCE)
FetchContent_Declare(
  freetype
  GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
  GIT_TAG VER-2-13-2 # (stable release, change if newer is needed)
)
FetchContent_MakeAvailable(freetype)

# ---- FMOD (Core/low-level) ----
option(ENGINE_ENABLE_FMOD "Enable FMOD audio" ON)
set(FMOD_SDK_DIR "${CMAKE_SOURCE_DIR}/fmod/api" CACHE PATH "Path to FMOD SDK root")


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
  target_link_libraries(${PROJECT_NAME} PRIVATE 
    glfw 
    libglew_static
    glm::glm 
    freetype 
    opengl32 
    user32 
    gdi32 
    shell32
  )
elseif(APPLE)
  target_link_libraries(${PROJECT_NAME} PRIVATE 
    glfw 
    libglew_static
    glm::glm 
    freetype 
    "-framework OpenGL"
  )
else()
  target_link_libraries(${PROJECT_NAME} PRIVATE 
    glfw 
    libglew_static
    glm::glm 
    freetype 
    GL
  )
endif()


if (ENGINE_ENABLE_FMOD)
  # --- Find headers (works for both 'core/inc' and 'inc')
  set(_FMOD_INC_CANDIDATES
    "${FMOD_SDK_DIR}/core/inc"
    "${FMOD_SDK_DIR}/inc"
  )
  set(FMOD_INCLUDE_DIR "")
  foreach(_cand IN LISTS _FMOD_INC_CANDIDATES)
    if (EXISTS "${_cand}/fmod.hpp")
      set(FMOD_INCLUDE_DIR "${_cand}")
      break()
    endif()
  endforeach()
  if (NOT FMOD_INCLUDE_DIR)
    message(FATAL_ERROR "FMOD headers not found in ${_FMOD_INC_CANDIDATES}")
  endif()

if (WIN32)
  # --- lib dir candidates (keep your x64 layout)
  set(_FMOD_LIBDIR_CANDIDATES
    "${FMOD_SDK_DIR}/core/lib/x64"
    "${FMOD_SDK_DIR}/lib/x64"
    "${FMOD_SDK_DIR}/lib"         # fallback
  )
  set(_FMOD_LIBDIR "")
  foreach(_cand IN LISTS _FMOD_LIBDIR_CANDIDATES)
    if (EXISTS "${_cand}/fmod_vc.lib" OR EXISTS "${_cand}/fmodL_vc.lib")
      set(_FMOD_LIBDIR "${_cand}")
      break()
    endif()
  endforeach()
  if (NOT _FMOD_LIBDIR)
    message(FATAL_ERROR "FMOD libs not found in: ${_FMOD_LIBDIR_CANDIDATES}")
  endif()

  set(FMOD_IMPLIB_DEBUG   "${_FMOD_LIBDIR}/fmodL_vc.lib")
  set(FMOD_IMPLIB_RELEASE "${_FMOD_LIBDIR}/fmod_vc.lib")
  set(FMOD_DLL_DEBUG      "${_FMOD_LIBDIR}/fmodL.dll")
  set(FMOD_DLL_RELEASE    "${_FMOD_LIBDIR}/fmod.dll")

  add_library(FMOD::fmod SHARED IMPORTED)
  set_target_properties(FMOD::fmod PROPERTIES
    IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo;MinSizeRel"
    INTERFACE_INCLUDE_DIRECTORIES "${FMOD_INCLUDE_DIR}"

    # Debug
    IMPORTED_IMPLIB_DEBUG               "${FMOD_IMPLIB_DEBUG}"
    IMPORTED_LOCATION_DEBUG             "${FMOD_DLL_DEBUG}"

    # Release
    IMPORTED_IMPLIB_RELEASE             "${FMOD_IMPLIB_RELEASE}"
    IMPORTED_LOCATION_RELEASE           "${FMOD_DLL_RELEASE}"

    # Map extra configs to Release (or switch to *_DEBUG if you prefer logging)
    IMPORTED_IMPLIB_RELWITHDEBINFO      "${FMOD_IMPLIB_RELEASE}"
    IMPORTED_LOCATION_RELWITHDEBINFO    "${FMOD_DLL_RELEASE}"
    IMPORTED_IMPLIB_MINSIZEREL          "${FMOD_IMPLIB_RELEASE}"
    IMPORTED_LOCATION_MINSIZEREL        "${FMOD_DLL_RELEASE}"
  )

  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<IF:$<CONFIG:Debug>,${FMOD_DLL_DEBUG},${FMOD_DLL_RELEASE}>"
      "$<TARGET_FILE_DIR:${PROJECT_NAME}>/")

  elseif(APPLE)
  set(_FMOD_LIBDIR_CANDIDATES
    "${FMOD_SDK_DIR}/core/lib"
    "${FMOD_SDK_DIR}/lib"
  )
  set(FMOD_DYLIB_RELEASE "")
  set(FMOD_DYLIB_DEBUG   "")
  foreach(_cand IN LISTS _FMOD_LIBDIR_CANDIDATES)
    if (EXISTS "${_cand}/libfmod.dylib"  AND NOT FMOD_DYLIB_RELEASE)
      set(FMOD_DYLIB_RELEASE "${_cand}/libfmod.dylib")
    endif()
    if (EXISTS "${_cand}/libfmodL.dylib" AND NOT FMOD_DYLIB_DEBUG)
      set(FMOD_DYLIB_DEBUG   "${_cand}/libfmodL.dylib")
    endif()
  endforeach()

  add_library(FMOD::fmod SHARED IMPORTED)
  set_target_properties(FMOD::fmod PROPERTIES
    IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo;MinSizeRel"
    INTERFACE_INCLUDE_DIRECTORIES "${FMOD_INCLUDE_DIR}"

    IMPORTED_LOCATION_RELEASE           "${FMOD_DYLIB_RELEASE}"
    IMPORTED_LOCATION_RELWITHDEBINFO    "${FMOD_DYLIB_RELEASE}"
    IMPORTED_LOCATION_MINSIZEREL        "${FMOD_DYLIB_RELEASE}"
  )
  # Use debug dylib if present; otherwise fall back to release
  if (FMOD_DYLIB_DEBUG)
    set_target_properties(FMOD::fmod PROPERTIES
      IMPORTED_LOCATION_DEBUG "${FMOD_DYLIB_DEBUG}")
  else()
    set_target_properties(FMOD::fmod PROPERTIES
      IMPORTED_LOCATION_DEBUG "${FMOD_DYLIB_RELEASE}")
  endif()

  set_target_properties(${PROJECT_NAME} PROPERTIES BUILD_RPATH "@loader_path")

 else() # Linux
  set(_FMOD_LIBDIR_CANDIDATES
    "${FMOD_SDK_DIR}/core/lib/x86_64"
    "${FMOD_SDK_DIR}/lib/x86_64"
    "${FMOD_SDK_DIR}/lib"
  )
  set(FMOD_SO_RELEASE "")
  set(FMOD_SO_DEBUG   "")
  foreach(_cand IN LISTS _FMOD_LIBDIR_CANDIDATES)
    if (EXISTS "${_cand}/libfmod.so"  AND NOT FMOD_SO_RELEASE)
      set(FMOD_SO_RELEASE "${_cand}/libfmod.so")
    endif()
    if (EXISTS "${_cand}/libfmodL.so" AND NOT FMOD_SO_DEBUG)
      set(FMOD_SO_DEBUG   "${_cand}/libfmodL.so")
    endif()
  endforeach()

  add_library(FMOD::fmod SHARED IMPORTED)
  set_target_properties(FMOD::fmod PROPERTIES
    IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo;MinSizeRel"
    INTERFACE_INCLUDE_DIRECTORIES "${FMOD_INCLUDE_DIR}"

    IMPORTED_LOCATION_RELEASE           "${FMOD_SO_RELEASE}"
    IMPORTED_LOCATION_RELWITHDEBINFO    "${FMOD_SO_RELEASE}"
    IMPORTED_LOCATION_MINSIZEREL        "${FMOD_SO_RELEASE}"
  )
  if (FMOD_SO_DEBUG)
    set_target_properties(FMOD::fmod PROPERTIES
      IMPORTED_LOCATION_DEBUG "${FMOD_SO_DEBUG}")
  else()
    set_target_properties(FMOD::fmod PROPERTIES
      IMPORTED_LOCATION_DEBUG "${FMOD_SO_RELEASE}")
  endif()

  set_target_properties(${PROJECT_NAME} PROPERTIES BUILD_RPATH "$ORIGIN")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE FMOD::fmod)

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

# --- copy assets into build/assets for Visual Studio ---
set(ENGINE_ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets")
if(EXISTS "${ENGINE_ASSETS_DIR}")
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${ENGINE_ASSETS_DIR}"
      "${CMAKE_BINARY_DIR}/assets")
endif()


