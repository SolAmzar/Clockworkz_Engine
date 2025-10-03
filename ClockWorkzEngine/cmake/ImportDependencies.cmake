# Maintained by Huang Po-Han

# This file contains macros to import GLFW, GLEW, GLM, FREETYPE, IMGUI

include(FetchContent)

# Define a consistent C++ standard for all dependencies
set(DEPENDENCIES_CXX_STANDARD 17 CACHE STRING "C++ standard used for all imported dependencies")

# Option to include ImGui demo windows
option(IMGUI_INCLUDE_DEMO "Include ImGui demo source (demo window)" OFF)

# Optional global override for all builds
option(IMPORT_ALL_IMGUI_DEMO "Include ImGui demo for all builds" OFF)
if(IMPORT_ALL_IMGUI_DEMO)
  set(IMGUI_INCLUDE_DEMO ON)
endif()

# ---- FMOD (Core/low-level) ----
option(ENGINE_ENABLE_FMOD "Enable FMOD audio" ON)
set(FMOD_SDK_DIR "${CMAKE_SOURCE_DIR}/fmod/api" CACHE PATH "Path to FMOD SDK root")

# ----------------------------------------------------------------------- MARCOS -----------------------------------------------------------------------------

# Macro to import GLFW into the project using FetchContent
# This avoids needing users to install GLFW manually on their system.
macro(import_glfw)

  # Prevent multiple inclusion:
  # If "glfw" is already imported as a CMake target, skip this block.
  if(NOT TARGET glfw)

    # Disable building extra stuff we don't need for our project:
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)        # Don't build GLFW documentation
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)       # Don't build GLFW's test programs
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)    # Toggle on to build GLFW example programs, off for don't.
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)           # Don't add "install" rules (we don't need system-wide install)
    set(GLFW_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE) # Toggle off for static/ on for dynamic

    # Tell CMake where to fetch GLFW from (GitHub repo + version tag)
    # GIT_TAG 3.4 means we are using the latest stable release.
    FetchContent_Declare(
      glfw
      GIT_REPOSITORY https://github.com/glfw/glfw.git
      GIT_TAG 3.4 # latest stable verison
    )
    
    # Download and make GLFW available as a CMake target.
    # After this, you can link against it using: target_link_libraries(your_target PRIVATE glfw)
    FetchContent_MakeAvailable(glfw)

    # Ensure the dependency uses the same C++ standard as the project
    set_property(TARGET glfw PROPERTY CXX_STANDARD ${DEPENDENCIES_CXX_STANDARD})
    set_property(TARGET glfw PROPERTY CXX_STANDARD_REQUIRED ON)

  endif()
endmacro()

# Macro to import GLEW via FetchContent
macro(import_glew)

  # If the CMake target "libglew_static" already exists,
  # it means GLEW is already imported, so skip everything.
  if(NOT TARGET libglew_static)

    # Don't build the extra GLEW command-line utilities (not needed).
    set(BUILD_UTILS OFF CACHE BOOL "" FORCE)

    # Tell the GLEW CMake project to build ONLY the static library,
    # not the shared/dynamic one.
    set(glew-cmake_BUILD_SHARED OFF CACHE BOOL "" FORCE)

    # Where to fetch GLEW from (official cmake fork).
    # "glew-cmake-2.2.0" is a stable tagged release.
    FetchContent_Declare(
      glew
      GIT_REPOSITORY https://github.com/Perlmint/glew-cmake.git
      GIT_TAG glew-cmake-2.2.0
    )

    # Actually download + add GLEW as a CMake target.
    FetchContent_MakeAvailable(glew)

    # Ensure the dependency uses the same C++ standard as the project
    set_property(TARGET libglew_static PROPERTY CXX_STANDARD ${DEPENDENCIES_CXX_STANDARD})
    set_property(TARGET libglew_static PROPERTY CXX_STANDARD_REQUIRED ON)

  endif()
endmacro()

# Macro to import GLM into the project using FetchContent
# GLM is a header-only math library (no .lib or .dll needed, just headers).
# This macro fetches the library automatically instead of requiring the user to install it.
macro(import_glm)

  # Prevent multiple inclusion:
  # If the CMake target "glm" already exists, skip everything inside.
  if(NOT TARGET glm)

    # Declare where to fetch GLM from:
    #   - "glm" here is just a local name/key used by CMake for this dependency
    #   - GIT_REPOSITORY is the official GitHub repo
    #   - GIT_TAG is the version
    FetchContent_Declare(
      glm
      GIT_REPOSITORY https://github.com/g-truc/glm.git
      GIT_TAG 1.0.1 # latest stable verison
    )

    # Actually download GLM and make it available.
    # After this line, you can link it with:
    #   target_link_libraries(your_project PRIVATE glm::glm)
    # Note: "glm::glm" is the target name defined inside GLM’s own CMakeLists.txt,
    #       it is not the same as the "glm" handle above.
    FetchContent_MakeAvailable(glm)

    # Ensure the dependency uses the same C++ standard as the project
    set_property(TARGET glm PROPERTY CXX_STANDARD ${DEPENDENCIES_CXX_STANDARD})
    set_property(TARGET glm PROPERTY CXX_STANDARD_REQUIRED ON)

  endif()
endmacro()

# Macro to import FreeType into the project using FetchContent
# FreeType is a library for loading and rendering fonts.
# This macro automatically fetches it, so users don't have to install it manually.
macro(import_freetype)

  # Guard: Only fetch FreeType if it hasn't been added already
  if(NOT TARGET freetype)

    # Optional dependency: HarfBuzz is used for advanced text shaping (e.g., complex scripts)
    # We usually disable it in game/engine projects to keep things simple.
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "Disable HarfBuzz" FORCE)
    
    # Declare where to fetch FreeType from:
    # - "freetype" is the local name/handle for CMake
    # - GIT_REPOSITORY is the official FreeType GitLab repo
    # - GIT_TAG VER-2-13-3 ensures we use a stable version
    FetchContent_Declare(
      freetype
      GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
      GIT_TAG VER-2-13-3
    )

    # Actually download and make FreeType available as a CMake target.
    # After this, you can link against it using:
    #   target_link_libraries(your_project PRIVATE freetype)
    FetchContent_MakeAvailable(freetype)

    # Ensure the dependency uses the same C++ standard as the project
    set_property(TARGET freetype PROPERTY CXX_STANDARD ${DEPENDENCIES_CXX_STANDARD})
    set_property(TARGET freetype PROPERTY CXX_STANDARD_REQUIRED ON)

  endif()
endmacro()

# Macro to import ImGui
# ImGui is a GUI library for C++ (e.g., used for debugging tools, editors, overlays)
macro(import_imgui)

  # Guard: If the CMake target "imgui" already exists, skip everything inside
  if(NOT TARGET imgui)

    # Tell CMake where to fetch ImGui from (GitHub + specific version)
    FetchContent_Declare(
      imgui
      GIT_REPOSITORY https://github.com/ocornut/imgui.git
      GIT_TAG v1.90.4
    )

    # Download ImGui and make it available for use
    FetchContent_MakeAvailable(imgui)

    # ImGui doesn't have a proper CMake project, so we need to manually create a library
    # List all source files that need to be compiled for ImGui
    set(IMGUI_SOURCES
      ${imgui_SOURCE_DIR}/imgui.cpp             # Core ImGui functionality
      ${imgui_SOURCE_DIR}/imgui_draw.cpp        # Drawing/rendering core
      ${imgui_SOURCE_DIR}/imgui_tables.cpp      # Tables support
      ${imgui_SOURCE_DIR}/imgui_widgets.cpp     # Widgets like buttons, sliders, etc.
      ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp  # Backend for GLFW (input handling)
      ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp # Backend for OpenGL3 rendering
    )

    # Optionally include demo
    if(IMGUI_INCLUDE_DEMO)
      list(APPEND IMGUI_SOURCES ${imgui_SOURCE_DIR}/imgui_demo.cpp) # Demo window (can be removed if not needed)
    endif()

    # Create a STATIC library target called "imgui"
    # STATIC means the code is compiled into your executable (not a separate .dll/.so)
    add_library(imgui STATIC ${IMGUI_SOURCES})

    # Tell CMake where to find ImGui headers when building your project
    target_include_directories(imgui PUBLIC 
      ${imgui_SOURCE_DIR}           # Core ImGui headers
      ${imgui_SOURCE_DIR}/backends  # Backend headers (GLFW + OpenGL)
    )

    # Link ImGui with GLFW and GLEW (required for input and OpenGL rendering)
    target_link_libraries(imgui PUBLIC glfw libglew_static)
    
    # Ensure ImGui uses the same C++ standard as the rest of the project
    # Needed because ImGui uses modern features like 'constexpr'
    set_property(TARGET imgui PROPERTY CXX_STANDARD ${DEPENDENCIES_CXX_STANDARD})
    set_property(TARGET imgui PROPERTY CXX_STANDARD_REQUIRED ON)

  endif()
endmacro()

macro(import_fmod target)
  if(NOT TARGET FMOD::fmod AND ENGINE_ENABLE_FMOD)
    # Locate headers
    set(_FMOD_INC_CANDIDATES
      "${FMOD_SDK_DIR}/core/inc"
      "${FMOD_SDK_DIR}/inc"
    )
    set(FMOD_INCLUDE_DIR "")
    foreach(_cand IN LISTS _FMOD_INC_CANDIDATES)
      if(EXISTS "${_cand}/fmod.hpp")
        set(FMOD_INCLUDE_DIR "${_cand}")
        break()
      endif()
    endforeach()
    if(NOT FMOD_INCLUDE_DIR)
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

    target_link_libraries(${target} PRIVATE FMOD::fmod)
  endif()
endmacro()

# Macro to import all dependencies
macro(importDependencies)
    message(STATUS "Starting to import dependencies...")

    message(STATUS "Importing GLFW...")
    import_glfw()
    message(STATUS "GLFW imported successfully.")

    message(STATUS "Importing GLEW...")
    import_glew()
    message(STATUS "GLEW imported successfully.")

    message(STATUS "Importing GLM...")
    import_glm()
    message(STATUS "GLM imported successfully.")

    message(STATUS "Importing FreeType...")
    import_freetype()
    message(STATUS "FreeType imported successfully.")

    # message(STATUS "Importing ImGui...")
    # import_imgui()
    # message(STATUS "ImGui imported successfully.")

    message(STATUS "All dependencies have been imported successfully.")
endmacro()