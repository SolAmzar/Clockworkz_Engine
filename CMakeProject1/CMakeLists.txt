cmake_minimum_required(VERSION 3.10)
project(CMakeProject1)

set(CMAKE_CXX_STANDARD 17)

# Fetch GLFW automatically
include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)

# Optional: Configure GLFW to not build extras
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Add executable
add_executable(CMakeProject1 main.cpp)

# Link libraries (note: just "glfw", not "glfw3")
target_link_libraries(CMakeProject1 PRIVATE 
    glfw 
    OpenGL::GL
)