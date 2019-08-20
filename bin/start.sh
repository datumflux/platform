#!/bin/sh

export ODBCSYSINI=$PWD/conf
export LD_LIBRARY_PATH=$PWD:$PWD/lib:$LD_LIBRARY_PATH

COMMON_LUA_PATH="./?.lua;./?/init.lua;deps/?.lua;deps/?/init.lua"

export LUA_PATH="${COMMON_LUA_PATH};lua/?.lua;lua/?/init.lua;lib/lua/?.lua;lib/lua/?/init.lua;lua/deps/?.lua;lua/deps/?/init.lua"
export LUA_CPATH="./?.so;lib/lua/?.so;lib/lua/loadall.so"
#
export LUAJIT_PATH="${COMMON_LUA_PATH};luajit/?.lua;luajit/?/init.lua;lib/luajit/?.lua;lib/luajit/?/init.lua;luajit/deps/?.lua;luajit/deps/?/init.lua"
export LUAJIT_CPATH="./?.so;lib/luajit/?.so;lib/luajit/loadall.so"

exec ./single -l conf/log4cxx.xml -c conf/stage.json -- $*
