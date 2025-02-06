#include "gm9lua.h"
#include "ui.h"
#include "language.h"
#ifndef NO_LUA
#include "fs.h"
#include "ff.h"
#include "vff.h"
#include "fsutil.h"
#include "unittype.h"
#include "nand.h"
#include "gm9loader.h"
#include "gm9os.h"
#include "gm9ui.h"
#include "gm9title.h"
#include "gm9internalfs.h"
#include "gm9internalsys.h"

#define DEBUGSP(x) ShowPrompt(false, (x))

typedef struct GM9LuaLoadF {
    int n; // pre-read characters
    FIL f;
    FRESULT res;
    char buff[BUFSIZ];
} GM9LuaLoadF;

// similar to "getF" in lauxlib.c
static const char* GetF(lua_State* L, void* ud, size_t* size) {
    GM9LuaLoadF* lf = (GM9LuaLoadF*)ud;
    UINT br = 0;
    (void)L; // unused
    if (lf->n > 0) { // check for pre-read characters
        *size = lf->n; // return those
        lf->n = 0;
    } else {
        if (fvx_eof(&lf->f)) return NULL;
        lf->res = fvx_read(&lf->f, lf->buff, BUFSIZ, &br);
        *size = (size_t)br;
        if (lf->res != FR_OK) return NULL;
    }
    return lf->buff;
}

// similar to "errfile" in lauxlib.c
static int ErrFile(lua_State* L, const char* what, int fnameindex, FRESULT res) {
    const char* filename = lua_tostring(L, fnameindex) + 1;
    lua_pushfstring(L, "cannot %s %s:\nfatfs error %d", what, filename, res);
    lua_remove(L, fnameindex);
    return LUA_ERRFILE;
}

int LoadLuaFile(lua_State* L, const char* filename) {
    GM9LuaLoadF lf;
    lf.n = 0;
    int status;
    int fnameindex = lua_gettop(L) + 1; // index of filename on the stack
    lua_pushfstring(L, "@%s", filename);
    lf.res = fvx_open(&lf.f, filename, FA_READ | FA_OPEN_EXISTING);
    if (lf.res != FR_OK) return ErrFile(L, "open", fnameindex, lf.res);
    
    status = lua_load(L, GetF, &lf, lua_tostring(L, -1), NULL);
    fvx_close(&lf.f);
    if (lf.res != FR_OK) {
        lua_settop(L, fnameindex);
        return ErrFile(L, "read", fnameindex, lf.res);
    }
    lua_remove(L, fnameindex);
    return status;
}

u32 GetFlagsFromTable(lua_State* L, int pos, u32 flags_ext_starter, u32 allowed_flags) {
    char types[FLAGS_COUNT][14] = { FLAGS_STR };
    int types_int[FLAGS_COUNT] = { FLAGS_CONSTS };
    u32 flags_ext = flags_ext_starter;

    for (int i = 0; i < FLAGS_COUNT; i++) {
        if (!(allowed_flags & types_int[i])) continue;
        lua_getfield(L, pos, types[i]);
        if (lua_toboolean(L, -1)) flags_ext |= types_int[i];
        lua_pop(L, 1);
    }

    return flags_ext;
}

void CheckWritePermissionsLuaError(lua_State* L, const char* path) {
    if (!CheckWritePermissions(path)) {
        luaL_error(L, "writing not allowed: %s", path);
    }
}

static const luaL_Reg gm9lualibs[] = {
    // built-ins
    {LUA_GNAME, luaopen_base},
    {LUA_LOADLIBNAME, luaopen_package},
    {LUA_COLIBNAME, luaopen_coroutine},
    {LUA_TABLIBNAME, luaopen_table},
    {LUA_STRLIBNAME, luaopen_string},
    {LUA_MATHLIBNAME, luaopen_math},
    {LUA_UTF8LIBNAME, luaopen_utf8},
    {LUA_DBLIBNAME, luaopen_debug},

    // gm9 custom
    {GM9LUA_OSLIBNAME, gm9lua_open_os},
    {GM9LUA_UILIBNAME, gm9lua_open_ui},
    {GM9LUA_TITLELIBNAME, gm9lua_open_title},

    // gm9 custom internals (usually wrapped by a pure lua module)
    {GM9LUA_INTERNALFSLIBNAME, gm9lua_open_internalfs},
    {GM9LUA_INTERNALSYSLIBNAME, gm9lua_open_internalsys},

    {NULL, NULL}
};

static void loadlibs(lua_State* L) {
    const luaL_Reg* lib;
    for (lib = gm9lualibs; lib->func; lib++) {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1); // remove lib from stack
    }
}

static bool RunFile(lua_State* L, const char* file) {
    int result = LoadLuaFile(L, file);
    if (result != LUA_OK) {
        char errstr[BUFSIZ] = {0};
        strlcpy(errstr, lua_tostring(L, -1), BUFSIZ);
        WordWrapString(errstr, 0);
        ShowPrompt(false, "Error during loading:\n%s", errstr);
        return false;
    }

    if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
        char errstr[BUFSIZ] = {0};
        strlcpy(errstr, lua_tostring(L, -1), BUFSIZ);
        WordWrapString(errstr, 0);
        ShowPrompt(false, "Error during execution:\n%s", errstr);
        return false;
    }

    return true;
}

// this is also taken from scripting.c
static inline bool isntrboot(void) {
    // taken over from Luma 3DS:
    // https://github.com/AuroraWright/Luma3DS/blob/bb5518b0f68d89bcd8efaf326355a770d5e57856/source/main.c#L58-L62
    const vu8 *bootMediaStatus = (const vu8 *) 0x1FFFE00C;
    const vu32 *bootPartitionsStatus = (const vu32 *) 0x1FFFE010;

    // shell closed, no error booting NTRCARD, NAND partitions not even considered
    return (bootMediaStatus[3] == 2) && !bootMediaStatus[1] && !bootPartitionsStatus[0] && !bootPartitionsStatus[1];
}

bool ExecuteLuaScript(const char* path_script) {
    lua_State* L = luaL_newstate();
    loadlibs(L);

    ResetPackageSearchersAndPath(L);
    ClearOutputBuffer();

    // current path
    char curr_dir[_VAR_CNT_LEN];
    if (path_script) {
        strncpy(curr_dir, path_script, _VAR_CNT_LEN);
        curr_dir[_VAR_CNT_LEN-1] = '\0';
        char* slash = strrchr(curr_dir, '/');
        if (slash) *slash = '\0';

        lua_pushstring(L, curr_dir);
    } else {
        lua_pushnil(L);
    }
    lua_setglobal(L, "CURRDIR");

    lua_pushliteral(L, VERSION);
    lua_setglobal(L, "GM9VER");

    lua_pushstring(L, path_script);
    lua_setglobal(L, "SCRIPT");

    lua_pushliteral(L, OUTPUT_PATH);
    lua_setglobal(L, "GM9OUT");

    lua_pushstring(L, IS_UNLOCKED ? (isntrboot() ? "ntrboot" : "sighax") : "");
    lua_setglobal(L, "HAX");

    lua_pushinteger(L, GetNandSizeSectors(NAND_SYSNAND) * 0x200);
    lua_setglobal(L, "NANDSIZE");

    lua_pushboolean(L, IS_DEVKIT);
    lua_setglobal(L, "IS_DEVKIT");

    lua_pushstring(L, IS_O3DS ? "O3DS" : "N3DS");
    lua_setglobal(L, "CONSOLE_TYPE");

    bool result = RunFile(L, "V:/preload.lua");
    if (!result) {
        ShowPrompt(false, "A fatal error happened in GodMode9's preload script.\n \nThis is not an error with your code, but with\nGodMode9. Please report it on GitHub.");
        lua_close(L);
        return false;
    }

    RunFile(L, path_script);

    lua_close(L);
    return true;
}
#else
// No-Lua version
bool ExecuteLuaScript(const char* path_script) {
    (void)path_script; // unused
    ShowPrompt(false, "%s", STR_LUA_NOT_INCLUDED);
    return false;
}
#endif
