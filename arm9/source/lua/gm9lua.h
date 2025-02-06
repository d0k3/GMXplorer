#pragma once
#include "common.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "scripting.h"

// this should probably go in filesys/fsutil.h
#define RECURSIVE       (1UL<<11)
#define TO_EMUNAND      (1UL<<12)
#define LEGIT           (1UL<<13)
#define FIND_FIRST      (1UL<<14)
#define INCLUDE_DIRS    (1UL<<15)
#define EXPLORER        (1UL<<16)
#define ENCRYPTED       (1UL<<17)

#define FLAGS_STR       "no_cancel", "silent", "calc_sha", "sha1", "skip", "overwrite", "append", "all", "recursive", "to_emunand", "legit", "first", "include_dirs", "explorer", "encrypted"
#define FLAGS_CONSTS    NO_CANCEL, SILENT, CALC_SHA, USE_SHA1, SKIP_ALL, OVERWRITE_ALL, APPEND_ALL, ASK_ALL, RECURSIVE, TO_EMUNAND, LEGIT, FIND_FIRST, INCLUDE_DIRS, EXPLORER, ENCRYPTED
#define FLAGS_COUNT     15

#define LUASCRIPT_EXT      "lua"
#define LUASCRIPT_MAX_SIZE STD_BUFFER_SIZE

// taken from arm9/source/utils/scripting.c
#define _VAR_CNT_LEN    256

#ifndef NO_LUA
static inline void CheckLuaArgCount(lua_State* L, int argcount, const char* cmd) {
    int args = lua_gettop(L);
    if (args != argcount) {
        luaL_error(L, "bad number of arguments passed to '%s' (expected %d, got %d)", cmd, argcount, args);
    }
}
// this is used in cases where a function accepts a flags table or something else
static inline bool CheckLuaArgCountPlusExtra(lua_State* L, int argcount, const char* cmd) {
    int args = lua_gettop(L);
    if (args != argcount && args != argcount + 1) {
        luaL_error(L, "bad number of arguments passed to '%s' (expected %d, got %d or %d)", cmd, argcount, args);
    }
    return args == argcount + 1;
}

int LoadLuaFile(lua_State* L, const char* filename);
u32 GetFlagsFromTable(lua_State* L, int pos, u32 flags_ext_starter, u32 allowed_flags);
void CheckWritePermissionsLuaError(lua_State* L, const char* path);
#endif
bool ExecuteLuaScript(const char* path_script);
