cmake_minimum_required(VERSION 3.10)
project(CMakeProject1)

set(CMAKE_CXX_STANDARD 17)

# Find required packages
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

# Add executable
add_executable(CMakeProject1 main.cpp)

# Link libraries
target_link_libraries(CMakeProject1 PRIVATE 
    glfw 
    OpenGL::GL
)