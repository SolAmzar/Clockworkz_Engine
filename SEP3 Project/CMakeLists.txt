# ======================= Project Configuration =========================
cmake_minimum_required(VERSION 2.8...3.13)

cmake_policy(VERSION 3.5)

project(GlWithCMake)

# Custom CMake modules
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(ImportDependencies)
importDependencies()

# Global compile flags and macros
add_definitions(-DGLEW_STATIC)
add_definitions(-DUSE_CSD3151_AUTOMATION=0)

# Include path for GLM
include_directories(${CMAKE_CURRENT_LIST_DIR}/lib/glm)

# ======================= Build Engine (All src together) =========================
file(GLOB_RECURSE ENGINE_SRC
    ${CMAKE_CURRENT_LIST_DIR}/src/*.[ch]pp
)

add_library(MyEngine ${ENGINE_SRC})

target_include_directories(MyEngine
    PUBLIC include
)

set_property(TARGET MyEngine PROPERTY CXX_STANDARD 20)

# Preserve warning flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(MyEngine PRIVATE -Wall ${DisableWarnings})
elseif(MSVC)
    target_compile_options(MyEngine PRIVATE /W3 /WX-)
endif()

# Link external dependencies
target_link_libraries(MyEngine
    PUBLIC glfw
    PUBLIC libglew_static
    PUBLIC imgui
    PUBLIC glm
)

# ======================= Copy Shaders =========================
file(GLOB SHADER_FILES "${CMAKE_CURRENT_LIST_DIR}/shaders/*")
if(SHADER_FILES)
    add_custom_command(TARGET MyEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:MyEngine>/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SHADER_FILES}
        $<TARGET_FILE_DIR:MyEngine>/shaders/
    )
endif()

# ======================= Build Executable =========================
add_executable(MyApp src/main.cpp)
target_link_libraries(MyApp PRIVATE MyEngine)
