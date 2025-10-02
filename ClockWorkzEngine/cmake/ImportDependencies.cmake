# This file contains macros to import GLFW, GLM, FREETYPE, GLEW, IMGUI

include(FetchContent)

# Macro to import GLFW into the project using FetchContent
# This avoids needing users to install GLFW manually on their system.
macro(import_glfw)
    # Prevent multiple inclusion:
    # If "glfw" is already imported as a CMake target, skip this block.
    if(NOT TARGET glfw)  # Guard to prevent multiple inclusion

        # Disable building extra stuff we don't need for our project:
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)        # Don't build GLFW documentation
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)       # Don't build GLFW's test programs
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)    # Toggle on to build GLFW example programs, off for don't.
        set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)           # Don't add "install" rules (we don't need system-wide install)
        set(GLFW_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE) # Toggle off for static/ on for dynamic

        # Tell CMake where to fetch GLFW from (GitHub repo + version tag)
        # GIT_TAG 3.4 means we are using the latest stable release (May 2024).
        FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 3.4 # latest stable verison
        )
        
        # Download and make GLFW available as a CMake target.
        # After this, you can link against it using: target_link_libraries(your_target PRIVATE glfw)
        FetchContent_MakeAvailable(glfw)

    endif()
endmacro()

# Macro to import glm
macro(import_glm)
    if(NOT TARGET glm)  # Guard to prevent multiple inclusion
        FetchContent_Declare(
            glm
            GIT_REPOSITORY https://github.com/g-truc/glm.git
            GIT_TAG master
        )
        FetchContent_MakeAvailable(glm)

    endif()
endmacro()


# Macro to import freetype
macro(import_freetype)
    if(NOT TARGET freetype)  # Guard to prevent multiple inclusion
        # Disable HarfBuzz (optional dependency)
        set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "Disable HarfBuzz" FORCE)
        
        FetchContent_Declare(
            freetype
            GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
            GIT_TAG VER-2-13-3 # <-- use this (latest stable as of 2025)
        )
        FetchContent_MakeAvailable(freetype)
    endif()
endmacro()


# Macro to import glew
macro(import_glew)
    if(NOT TARGET glew_s)  # Guard to prevent multiple inclusion

        FetchContent_Declare(
            glew
            GIT_REPOSITORY https://github.com/omniavinco/glew-cmake.git
            GIT_TAG master  # Or use a specific commit or tag if preferred
        )

        # Disable shared builds if needed
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libs" FORCE)
        FetchContent_MakeAvailable(glew)


    endif()
endmacro()

# Macro to import ImGui
macro(import_imgui)
    if(NOT TARGET imgui)  # Guard to prevent multiple inclusion
        FetchContent_Declare(
            imgui
            GIT_REPOSITORY https://github.com/ocornut/imgui.git
            GIT_TAG v1.90.4
        )
        FetchContent_MakeAvailable(imgui)

        # Create ImGui library manually since it doesn't have CMakeLists.txt
        set(IMGUI_SOURCES
            ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_demo.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        )

        add_library(imgui STATIC ${IMGUI_SOURCES})
        target_include_directories(imgui PUBLIC 
            ${imgui_SOURCE_DIR}
            ${imgui_SOURCE_DIR}/backends
        )
        target_link_libraries(imgui PUBLIC glfw libglew_static)
        
        # Set C++ standard for ImGui (required for constexpr support)
        set_property(TARGET imgui PROPERTY CXX_STANDARD 11)
        set_property(TARGET imgui PROPERTY CXX_STANDARD_REQUIRED ON)
    endif()
endmacro()

# Macro to import all dependencies
macro(importDependencies)
    message(STATUS "Starting to import dependencies...")

    message(STATUS "Importing GLFW...")
    import_glfw()
    message(STATUS "GLFW imported successfully.")

    message(STATUS "Importing GLM...")
    import_glm()
    message(STATUS "GLM imported successfully.")

    message(STATUS "Importing GLEW...")
    import_glew()
    message(STATUS "GLEW imported successfully.")

    message(STATUS "Importing FreeType...")
    import_freetype()
    message(STATUS "FreeType imported successfully.")

    message(STATUS "Importing ImGui...")
    import_imgui()
    message(STATUS "ImGui imported successfully.")

    message(STATUS "All dependencies have been imported successfully.")
endmacro()