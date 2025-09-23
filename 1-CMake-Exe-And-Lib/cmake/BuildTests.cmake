MACRO(SETUP_EXAMPLE APP_NAME)

set(GAME_ENGINE_NAME MyEngine)

# Project name
project(${APP_NAME} VERSION 1.0.0 LANGUAGES CXX)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add the executable
file(GLOB_RECURSE ${APP_NAME}_source_files 
    ${CMAKE_CURRENT_LIST_DIR}/${APP_NAME}/*.[ch]pp
)

add_executable(${APP_NAME} 
    ${${APP_NAME}_source_files} 
)

# Set common include directories
target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_definitions(${APP_NAME} PRIVATE ASSETS_DIR_PATH="${CMAKE_SOURCE_DIR}/Engine/Assets/")
message(STATUS "Using MyEngine lib from build directory....")

# Link the library to the executable using the CMake target
target_link_libraries(${APP_NAME} PRIVATE 
    ${GAME_ENGINE_NAME}
)

ENDMACRO(SETUP_EXAMPLE)