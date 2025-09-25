# Sandbox / Projects CMakeLists.txt

# Macro to set up a game executable
macro(SETUP_GAME APP_NAME)
    # Project name
    project(${APP_NAME} VERSION 1.0 LANGUAGES CXX)

    # Collect all source files recursively
    file(GLOB_RECURSE ${APP_NAME}_SRC
        ${CMAKE_CURRENT_LIST_DIR}/${APP_NAME}/src/*.[ch]pp
        ${CMAKE_CURRENT_LIST_DIR}/${APP_NAME}/include/*.[ch]pp
    )

    # Create the executable
    add_executable(${APP_NAME} ${${APP_NAME}_SRC})

    # Add include directories
    target_include_directories(${APP_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/${APP_NAME}/include
        ${CMAKE_SOURCE_DIR}/Engine  # Engine headers
    )

    # Link to the engine
    target_link_libraries(${APP_NAME} PRIVATE MyEngine)

    # Copy shaders to the build folder
    file(GLOB ${APP_NAME}_SHADERS
        ${CMAKE_CURRENT_LIST_DIR}/${APP_NAME}/shaders/*.* 
    )

    if(${APP_NAME}_SHADERS)
        add_custom_command(TARGET ${APP_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:${APP_NAME}>/shaders
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${${APP_NAME}_SHADERS}
            $<TARGET_FILE_DIR:${APP_NAME}>/shaders/
        )
    endif()

    # Set C++ standard
    set_property(TARGET ${APP_NAME} PROPERTY CXX_STANDARD 20)
endmacro()

# Automatically set up all games in this folder
file(GLOB GAMES RELATIVE ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/*)

foreach(game ${GAMES})
    if(IS_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/${game}")
        message(STATUS "Configuring game: ${game}")
        SETUP_GAME(${game})
    endif()
endforeach()
