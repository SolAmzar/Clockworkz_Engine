# ImportLua.cmake - Fetch and build Lua 5.4.8 + Sol2 for ClockWorkz Engine

function(import_lua TARGET_NAME)
    include(FetchContent)

    message(STATUS "Fetching Lua 5.4.8...")
    
    # Fetch Lua 5.4.8 source
    FetchContent_Declare(
        lua_source
        URL https://www.lua.org/ftp/lua-5.4.8.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    # Fetch Sol2 (header-only Lua binding library)
    FetchContent_Declare(
        sol2
        GIT_REPOSITORY https://github.com/ThePhD/sol2.git
        GIT_TAG v3.3.1
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(lua_source sol2)

    # Build Lua as a static library
    set(LUA_SRC_DIR ${lua_source_SOURCE_DIR}/src)
    
    set(LUA_CORE_SOURCES
        ${LUA_SRC_DIR}/lapi.c
        ${LUA_SRC_DIR}/lcode.c
        ${LUA_SRC_DIR}/lctype.c
        ${LUA_SRC_DIR}/ldebug.c
        ${LUA_SRC_DIR}/ldo.c
        ${LUA_SRC_DIR}/ldump.c
        ${LUA_SRC_DIR}/lfunc.c
        ${LUA_SRC_DIR}/lgc.c
        ${LUA_SRC_DIR}/llex.c
        ${LUA_SRC_DIR}/lmem.c
        ${LUA_SRC_DIR}/lobject.c
        ${LUA_SRC_DIR}/lopcodes.c
        ${LUA_SRC_DIR}/lparser.c
        ${LUA_SRC_DIR}/lstate.c
        ${LUA_SRC_DIR}/lstring.c
        ${LUA_SRC_DIR}/ltable.c
        ${LUA_SRC_DIR}/ltm.c
        ${LUA_SRC_DIR}/lundump.c
        ${LUA_SRC_DIR}/lvm.c
        ${LUA_SRC_DIR}/lzio.c
        ${LUA_SRC_DIR}/lauxlib.c
        ${LUA_SRC_DIR}/lbaselib.c
        ${LUA_SRC_DIR}/lcorolib.c
        ${LUA_SRC_DIR}/ldblib.c
        ${LUA_SRC_DIR}/liolib.c
        ${LUA_SRC_DIR}/lmathlib.c
        ${LUA_SRC_DIR}/loadlib.c
        ${LUA_SRC_DIR}/loslib.c
        ${LUA_SRC_DIR}/lstrlib.c
        ${LUA_SRC_DIR}/ltablib.c
        ${LUA_SRC_DIR}/lutf8lib.c
        ${LUA_SRC_DIR}/linit.c
    )

    add_library(lua_static STATIC ${LUA_CORE_SOURCES})
    
    target_include_directories(lua_static PUBLIC ${LUA_SRC_DIR})
    
    # Platform-specific definitions
    if(UNIX AND NOT APPLE)
        target_compile_definitions(lua_static PUBLIC LUA_USE_LINUX)
        target_link_libraries(lua_static PUBLIC m dl)
    elseif(APPLE)
        target_compile_definitions(lua_static PUBLIC LUA_USE_MACOSX)
        target_link_libraries(lua_static PUBLIC m dl)
    elseif(WIN32)
        # No special linking needed for Windows
        target_compile_definitions(lua_static PUBLIC LUA_USE_WINDOWS)
    endif()

    # Disable warnings for Lua (third-party code)
    if(MSVC)
        target_compile_options(lua_static PRIVATE /W0)
    else()
        target_compile_options(lua_static PRIVATE -w)
    endif()

    # Link Lua and Sol2 to the target (PUBLIC so headers propagate to consumers)
    target_link_libraries(${TARGET_NAME} PUBLIC lua_static)
    target_link_libraries(${TARGET_NAME} PUBLIC sol2::sol2)

    # Mark Lua and Sol2 headers as SYSTEM so IntelliSense suppresses errors from them
    target_include_directories(${TARGET_NAME} SYSTEM PUBLIC
        ${LUA_SRC_DIR}
        ${sol2_SOURCE_DIR}/include
    )

    # Also pass /external:I for MSVC compiler warning suppression
    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE
            /external:W0
            "/external:I${sol2_SOURCE_DIR}/include"
        )
    endif()
    
    message(STATUS "Lua 5.4.8 and Sol2 successfully configured")
endfunction()
