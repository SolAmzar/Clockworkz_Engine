# Engine CMakeLists.txt for modular subsystems

set(ENGINE_NAME MyEngine)

# Gather all .cpp files from all subsystem src/ folders
file(GLOB_RECURSE ENGINE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/*/src/*.[ch]pp
)

# Create the library
add_library(${ENGINE_NAME} ${ENGINE_SRC})

# Set C++ standard
set_property(TARGET ${ENGINE_NAME} PROPERTY CXX_STANDARD 20)

# Compiler warnings (unchanged)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${ENGINE_NAME} PRIVATE -Wall ${DisableWarnings})
elseif(MSVC)
    target_compile_options(${ENGINE_NAME} PRIVATE /W3 /WX-)
endif()

# Collect include directories (folders only)
file(GLOB ENGINE_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/*/include
)

# Add them as include directories
target_include_directories(${ENGINE_NAME} PRIVATE ${ENGINE_INCLUDE_DIRS})
