#ifndef NO_LUA
#include "gm9lua.h"
#include "utils.h"
#include "fs.h"
#include "language.h"
#include "ui.h"
#include "game.h"
#include "ips.h"
#include "bps.h"

static int title_decrypt(lua_State* L) {
    CheckLuaArgCount(L, 1, "title.decrypt");
    const char* path = luaL_checkstring(L, 1);
    const char* whichfailed = "";

    u64 filetype = IdentifyFileType(path);
    bool ret;
    if (filetype & BIN_KEYDB) {
        ret = (CryptAesKeyDb(path, true, false) == 0);
        whichfailed = "CryptAesKeyDb";
    } else {
        ret = (CryptGameFile(path, true, false) == 0);
        whichfailed = "CryptGameFile";
    }

    if (!ret) {
        return luaL_error(L, "%s failed on %s", whichfailed, path);
    }

    return 0;
}

static int title_encrypt(lua_State* L) {
    CheckLuaArgCount(L, 1, "title.encrypt");
    const char* path = luaL_checkstring(L, 1);
    const char* whichfailed = "";

    u64 filetype = IdentifyFileType(path);
    bool ret;
    if (filetype & BIN_KEYDB) {
        ret = (CryptAesKeyDb(path, true, true) == 0);
        whichfailed = "CryptAesKeyDb";
    } else {
        ret = (CryptGameFile(path, true, true) == 0);
        whichfailed = "CryptGameFile";
    }

    if (!ret) {
        return luaL_error(L, "%s failed on %s", whichfailed, path);
    }

    return 0;
}

static int title_install(lua_State* L) {
    bool extra = CheckLuaArgCountPlusExtra(L, 1, "title.install");
    const char* path = luaL_checkstring(L, 1);

    u32 flags = 0;
    if (extra) {
        flags = GetFlagsFromTable(L, 2, flags, TO_EMUNAND);
    };

    bool ret = (InstallGameFile(path, (flags & TO_EMUNAND)) == 0);
    if (!ret) {
        return luaL_error(L, "InstallGameFile failed on %s", path);
    }

    return 0;
}

static int title_build_cia(lua_State* L) {
    bool extra = CheckLuaArgCountPlusExtra(L, 1, "title.build_cia");
    const char* path = luaL_checkstring(L, 1);

    u32 flags = 0;
    if (extra) {
        flags = GetFlagsFromTable(L, 2, flags, LEGIT);
    };

    bool ret = (BuildCiaFromGameFile(path, (flags & LEGIT)) == 0);
    if (!ret) {
        return luaL_error(L, "BuildCiaFromGameFile failed on %s", path);
    }

    return 0;
}

static int title_extract_code(lua_State* L) {
    CheckLuaArgCount(L, 2, "title.extract_code");
    const char* path_src = luaL_checkstring(L, 1);
    const char* path_dst = luaL_checkstring(L, 2);

    u64 filetype = IdentifyFileType(path_src);
    if (!FTYPE_HASCODE(filetype)) {
        return luaL_error(L, "%s does not have code", path_src);
    } else {
        CheckWritePermissionsLuaError(L, path_dst);
        ShowString("%s", STR_EXTRACTING_DOT_CODE);
        bool ret = (ExtractCodeFromCxiFile(path_src, path_dst, NULL) == 0);
        if (!ret) {
            return luaL_error(L, "failed to extract code from %s", path_src);
        }
    }

    return 0;
}

static int title_compress_code(lua_State* L) {
    CheckLuaArgCount(L, 2, "title.compress_code");
    const char* path_src = luaL_checkstring(L, 1);
    const char* path_dst = luaL_checkstring(L, 2);

    CheckWritePermissionsLuaError(L, path_dst);
    ShowString("%s", STR_COMPRESSING_DOT_CODE);
    bool ret = (CompressCode(path_src, path_dst) == 0);
    if (!ret) {
        return luaL_error(L, "failed to compress code from %s", path_src);
    }

    return 0;
}

static int title_apply_ips(lua_State* L) {
    CheckLuaArgCount(L, 3, "title.apply_ips");
    const char* path_patch = luaL_checkstring(L, 1);
    const char* path_src = luaL_checkstring(L, 2);
    const char* path_target = luaL_checkstring(L, 3);

    bool ret = (ApplyIPSPatch(path_patch, path_src, path_target) == 0);
    if (!ret) {
        return luaL_error(L, "ApplyIPSPatch failed");
    }

    return 0;
}

static int title_apply_bps(lua_State* L) {
    CheckLuaArgCount(L, 3, "title.apply_bps");
    const char* path_patch = luaL_checkstring(L, 1);
    const char* path_src = luaL_checkstring(L, 2);
    const char* path_target = luaL_checkstring(L, 3);

    bool ret = (ApplyBPSPatch(path_patch, path_src, path_target) == 0);
    if (!ret) {
        return luaL_error(L, "ApplyBPSPatch failed");
    }

    return 0;
}

static int title_apply_bpm(lua_State* L) {
    CheckLuaArgCount(L, 3, "title.apply_bpm");
    const char* path_patch = luaL_checkstring(L, 1);
    const char* path_src = luaL_checkstring(L, 2);
    const char* path_target = luaL_checkstring(L, 3);

    bool ret = (ApplyBPMPatch(path_patch, path_src, path_target) == 0);
    if (!ret) {
        return luaL_error(L, "ApplyBPMPatch failed");
    }

    return 0;
}

static const luaL_Reg title_lib[] = {
    {"decrypt", title_decrypt},
    {"encrypt", title_encrypt},
    {"install", title_install},
    {"build_cia", title_build_cia},
    {"extract_code", title_extract_code},
    {"compress_code", title_compress_code},
    {"apply_ips", title_apply_ips},
    {"apply_bps", title_apply_bps},
    {"apply_bpm", title_apply_bpm},
    {NULL, NULL}
};

int gm9lua_open_title(lua_State* L) {
    luaL_newlib(L, title_lib);
    return 1;
}
#endif
