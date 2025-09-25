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

# ======================= Build Engine =========================
add_subdirectory(Engine)

# External libraries (including the engine library itself)
set(ALL_LIBS
    PRIVATE glfw
    PRIVATE libglew_static
    PRIVATE imgui
    PRIVATE MyEngine       # Engine library
)

# ======================= AUTO GAME DISCOVERY =========================
file(GLOB children RELATIVE "${CMAKE_CURRENT_LIST_DIR}/Game" "${CMAKE_CURRENT_LIST_DIR}/Game/*")

foreach(child ${children})
    if(IS_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/Game/${child}")
        message(STATUS "Configuring game: ${child}")

        # Collect source files
        file(GLOB_RECURSE ${child}_source_files
            ${CMAKE_CURRENT_LIST_DIR}/Game/${child}/src/*.cpp
            ${CMAKE_CURRENT_LIST_DIR}/Game/${child}/include/*.h
            ${CMAKE_CURRENT_LIST_DIR}/Game/${child}/include/*.hpp
        )

        add_executable(${child} ${${child}_source_files})

        # Copy shader files
        file(GLOB ${child}_shader_files "${CMAKE_CURRENT_LIST_DIR}/Game/${child}/shaders/*")
        if(${child}_shader_files)
            add_custom_command(TARGET ${child} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                $<TARGET_FILE_DIR:${child}>/shaders
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${${child}_shader_files}
                $<TARGET_FILE_DIR:${child}>/shaders/
            )
        endif()

        # Link libraries
        target_link_libraries(${child} ${ALL_LIBS})

        # Set C++ standard
        set_property(TARGET ${child} PROPERTY CXX_STANDARD 20)

        # Preserve warning flags exactly as before
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${child} PRIVATE -Wall ${DisableWarnings})
        elseif(MSVC)
            target_compile_options(${child} PRIVATE /W3 /WX-)
        endif()

        # Include directories
        target_include_directories(${child} PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/Game/${child}/include
            ${CMAKE_CURRENT_LIST_DIR}/Engine/Graphics/include
        )
    endif()
endforeach()

# ======================= END AUTO CONFIGURATION =========================
