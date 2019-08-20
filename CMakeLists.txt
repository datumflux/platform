# CentOS의 경우 -- sudo yum install cmake3 설치 후
cmake_minimum_required(VERSION 3.1)
project(Single)

set(CMAKE_INCLUDE_PATH
    src
    include
    stage
    core/include
    tinyxml2
    /usr/include/jsoncpp
    ${CMAKE_INCLUDE_PATH})

set(CMAKE_LIBRARY_PATH
    lua/5.3/lib
    lua/jit/lib
    ${CMAKE_LIBRARY_PATH})

set(LINK_LIBRARY_FILES
    log4cxx
    aprutil-1
    crypt
    ldap_r
    lber
    expat
    db-5.3
    apr-1
    jsoncpp
    z
    zip
    jemalloc
    pthread
    stdc++
    dl
    ${LINK_LIBRARY_FILES})

set(SOURCE_DIRS
    src
    core/libsrc
    tinyxml2
    ${SOURCE_DIRS})

# set(SOURCE_FILES main.cpp)
foreach (DIR ${SOURCE_DIRS})
    file(GLOB_RECURSE
        SRCS
        ${PROJECT_SOURCE_DIR}/${DIR}/*.c*)
    set(SOURCE_FILES ${SOURCE_FILES} ${SRCS})
endforeach ()
set(SOURCE_FILES ${SOURCE_FILES} stage/route.cpp stage/ticket.cpp)

#
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -fPIC -rdynamic -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC -rdynamic -Wall")

# set(LINK_LIBRARY_FILES debug asan ${LINK_LIBRARY_FILES})
# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb -fsanitize=address")
set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG ${CMAKE_CXX_FLAGS_DEBUG} -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "-DRELEASE ${CMAKE_CXX_FLAGS_RELEASE}")

add_definitions(
        -D_GNU_SOURCE
        -D__STDC_LIMIT_MACROS
        -D_REENTRANT
        -DLUA_BUNDLE
)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

include_directories(${CMAKE_INCLUDE_PATH})
link_directories(${CMAKE_LIBRARY_PATH})
#
#
set(CMAKE_SHARED_LIBRARY_PREFIX "")

list(FILTER SOURCE_FILES EXCLUDE REGEX ".*_debug.c$")
#
# stage.lua
set(LUA_INCLUDE_PATH lua/helper lua/lua-compat-5.3/c-api)

# set(LUA_BUNDLE_LIBRARY cryptopp)
# set(LUA_BUNDLE_SOURCES stage/lua/lbrokerlib.cpp)
#
#
#
foreach (DIR lua/helper)
    file(GLOB_RECURSE
            SRCS
            ${PROJECT_SOURCE_DIR}/${DIR}/*.c*)
    set(LUA_SOURCE_FILES ${LUA_SOURCE_FILES} ${SRCS})
endforeach ()
set(LUA_SOURCE_FILES ${LUA_SOURCE_FILES} ${LUA_BUNDLE_SOURCES} stage/lua.cpp)

if(IS_DIRECTORY "${PROJECT_SOURCE_DIR}/lua/5.3/include")
    add_library(stage_lua SHARED ${LUA_SOURCE_FILES})
    set_target_properties(stage_lua PROPERTIES OUTPUT_NAME "lua")
    target_include_directories(stage_lua PUBLIC lua/5.3/include ${LUA_INCLUDE_PATH})
    target_link_libraries(stage_lua lua-5.3 ${LUA_BUNDLE_LIBRARY})
    #
    #
    add_library(lua_broker SHARED stage/lua/lbrokerlib.cpp)
    set_target_properties(lua_broker PROPERTIES OUTPUT_NAME "lib/lua/broker")
    target_include_directories(lua_broker PUBLIC lua/5.3/include ${LUA_INCLUDE_PATH})
    target_link_libraries(lua_broker cryptopp)

    add_library(lua_bundle SHARED stage/lua/lbundlelib.cpp stage/libsrc/_well512.c)
    set_target_properties(lua_bundle PROPERTIES OUTPUT_NAME "lib/lua/bundle")
    target_include_directories(lua_bundle PUBLIC lua/5.3/include ${LUA_INCLUDE_PATH})
    target_link_libraries(lua_bundle )

    add_library(lua_logger SHARED stage/lua/lloggerlib.cpp)
    set_target_properties(lua_logger PROPERTIES OUTPUT_NAME "lib/lua/logger")
    target_include_directories(lua_logger PUBLIC lua/5.3/include ${LUA_INCLUDE_PATH})
    target_link_libraries(lua_logger )

    add_library(lua_odbc SHARED stage/lua/lodbclib.cpp stage/libsrc/odbc.cpp)
    set_target_properties(lua_odbc PROPERTIES OUTPUT_NAME "lib/lua/odbc")
    target_include_directories(lua_odbc PUBLIC lua/5.3/include ${LUA_INCLUDE_PATH})
    target_link_libraries(lua_odbc odbc)
endif()

#
# stage.luajit
if(IS_DIRECTORY "${PROJECT_SOURCE_DIR}/lua/jit/include")
    add_library(stage_luajit SHARED ${LUA_SOURCE_FILES})
    set_target_properties(stage_luajit PROPERTIES OUTPUT_NAME "luajit")
    target_include_directories(stage_luajit PUBLIC lua/jit/include ${LUA_INCLUDE_PATH})
    target_link_libraries(stage_luajit luajit-5.1 ${LUA_BUNDLE_LIBRARY})

    #
    #
    add_library(luajit_broker SHARED stage/lua/lbrokerlib.cpp)
    set_target_properties(luajit_broker PROPERTIES OUTPUT_NAME "lib/luajit/broker")
    target_include_directories(luajit_broker PUBLIC lua/jit/include ${LUA_INCLUDE_PATH})
    target_link_libraries(luajit_broker cryptopp)

    add_library(luajit_bundle SHARED stage/lua/lbundlelib.cpp stage/libsrc/_well512.c)
    set_target_properties(luajit_bundle PROPERTIES OUTPUT_NAME "lib/luajit/bundle")
    target_include_directories(luajit_bundle PUBLIC lua/jit/include ${LUA_INCLUDE_PATH})
    target_link_libraries(luajit_bundle )

    add_library(luajit_logger SHARED stage/lua/lloggerlib.cpp)
    set_target_properties(luajit_logger PROPERTIES OUTPUT_NAME "lib/luajit/logger")
    target_include_directories(luajit_logger PUBLIC lua/jit/include ${LUA_INCLUDE_PATH})
    target_link_libraries(luajit_logger )

    add_library(luajit_odbc SHARED stage/lua/lodbclib.cpp stage/libsrc/odbc.cpp)
    set_target_properties(luajit_odbc PROPERTIES OUTPUT_NAME "lib/luajit/odbc")
    target_include_directories(luajit_odbc PUBLIC lua/jit/include ${LUA_INCLUDE_PATH})
    target_link_libraries(luajit_odbc odbc)
endif()
#
# stage.curl
add_library(stage_curl SHARED stage/curl.cpp)
set_target_properties(stage_curl PROPERTIES OUTPUT_NAME "curl")
target_link_libraries(stage_curl curl)
#
# stage.index
add_library(stage_index SHARED stage/libsrc/index.cpp stage/index.cpp)
set_target_properties(stage_index PROPERTIES OUTPUT_NAME "index")
#
# single
#list(FILTER SOURCE_FILES EXCLUDE REGEX ".*index.cpp$")
add_executable(single ${SOURCE_FILES})
target_link_libraries(single ${LINK_LIBRARY_FILES})
