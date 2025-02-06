#ifndef NO_LUA
#include "gm9loader.h"
#include "gm9lua.h"
#include "vff.h"
#include "ui.h"

// a lot of this code is based on stuff in loadlib.c but adapted for GM9

// similar to readable
static int Readable(const char* filename) {
    FIL f;
    FRESULT res = fvx_open(&f, filename, FA_READ | FA_OPEN_EXISTING);
    if (res != FR_OK) return 0;
    fvx_close(&f);
    return 1;
}

// similar to getnextfilename
static const char* GetNextFileName(char** path, char* end) {
    char *sep;
    char *name = *path;
    if (name == end)
        return NULL;  /* no more names */
    else if (*name == '\0') {  /* from previous iteration? */
        *name = *LUA_PATH_SEP;  /* restore separator */
        name++;  /* skip it */
    }
    sep = strchr(name, *LUA_PATH_SEP);  /* find next separator */
    if (sep == NULL)  /* separator not found? */
        sep = end;  /* name goes until the end */
    *sep = '\0';  /* finish file name */
    *path = sep;  /* will start next search from here */
    return name;
}

// similar to pusherrornotfound
static void PushErrorNotFound(lua_State* L, const char* path) {
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    luaL_addstring(&b, "no file '");
    luaL_addgsub(&b, path, LUA_PATH_SEP, "'\n\tno file '");
    luaL_addstring(&b, "'");
    luaL_pushresult(&b);
}

// similar to searchpath
static const char* SearchPath(lua_State* L, const char* name, const char* path, const char* sep) {
    luaL_Buffer buff;
    char* pathname;
    char* endpathname;
    const char* filename;
    if (*sep != '\0' && strchr(name, *sep) != NULL)
        name = luaL_gsub(L, name, sep, "/");

    luaL_buffinit(L, &buff);
    // add path to the buffer, replacing marks ('?') with the file name
    luaL_addgsub(&buff, path, LUA_PATH_MARK, name);
    luaL_addchar(&buff, '\0');
    pathname = luaL_buffaddr(&buff);
    endpathname = pathname + luaL_bufflen(&buff) + 1;
    while ((filename = GetNextFileName(&pathname, endpathname)) != NULL) {
        if (Readable(filename))
            return lua_pushstring(L, filename);
    }
    luaL_pushresult(&buff);
    PushErrorNotFound(L, lua_tostring(L, -1));
    return NULL;
}

// similar to findfile
static const char* FindLuaFile(lua_State* L, const char* name, const char* pname) {
    const char* path;
    lua_getfield(L, lua_upvalueindex(1), pname); // gets 'package' table
    path = lua_tostring(L, -1);
    if (path == NULL) luaL_error(L, "'package.%s' must be a string", pname);
    return SearchPath(L, name, path, ".");
}

// similar to checkload
static int CheckLoad(lua_State* L, int stat, const char* filename) {
    if (stat) {
        lua_pushstring(L, filename);
        return 2; // return open function and filename
    } else {
        return luaL_error(L, "error loading module '%s' from file '%s':\n\t%s",
                             lua_tostring(L, 1), filename, lua_tostring(L, -1));
    }
}

// similar to searcher_Lua
static int PackageSearcher(lua_State* L) {
    const char *filename;
    const char *name = luaL_checkstring(L, 1);

    filename = FindLuaFile(L, name, "path");

    if (filename == NULL) return 1;  // module not found in this path
    return CheckLoad(L, (LoadLuaFile(L, filename) == LUA_OK), filename);
}

void ResetPackageSearchersAndPath(lua_State* L) {
    // get package module
    lua_getglobal(L, "package");
    
    // the default package.path only makes sense on a full OS
    // maybe this should include the lua script's current directory somehow...
    lua_pushliteral(L, GM9LUA_DEFAULT_PATH);
    lua_setfield(L, -2, "path");

    // package.cpath is for loading binary modules, useless on GM9
    lua_pushliteral(L, "");
    lua_setfield(L, -2, "cpath");

    // the default package searchers only make sense on a full OS
    // so here we replace the lua loader with a custom one, and remove the C/Croot loaders
    // leaving the initial one (preload)
    lua_getfield(L, -1, "searchers");
    lua_pushvalue(L, -2); // copy 'package' to the top of the stack, to set 'package' as upvalue for all searchers
    lua_pushcclosure(L, PackageSearcher, 1); // push PackageSearcher with one upvalue being the "packages" module/table
    lua_rawseti(L, -2, 2); // replace default lua loader
    lua_pushnil(L);
    lua_rawseti(L, -2, 3); // remove C loader
    lua_pushnil(L);
    lua_rawseti(L, -2, 4); // remove C root loader
    lua_pop(L, 1); // remove "searchers"
    lua_pop(L, 1); // remove "packages"
}
#endif
