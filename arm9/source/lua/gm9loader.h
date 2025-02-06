#include "gm9lua.h"

/*
 * 0:/gm9/luapackages/?.lua;
 * 0:/gm9/luapackages/?/init.lua;
 * V:/luapackages/?.lua;
 * V:/luapackages/?/init.lua
 */
#define GM9LUA_DEFAULT_PATH \
        "0:/gm9/luapackages/"LUA_PATH_MARK".lua" LUA_PATH_SEP \
        "0:/gm9/luapackages/"LUA_PATH_MARK"/init.lua" LUA_PATH_SEP \
        "V:/luapackages/"LUA_PATH_MARK".lua" LUA_PATH_SEP \
        "V:/luapackages/"LUA_PATH_MARK"/init.lua"

void ResetPackageSearchersAndPath(lua_State* L);
