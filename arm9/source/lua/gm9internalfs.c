#ifndef NO_LUA
#include "gm9internalfs.h"
#include "fs.h"
#include "ui.h"
#include "utils.h"
#include "sha.h"
#include "nand.h"
#include "language.h"
#include "hid.h"
#include "game.h"
#include "gamecart.h"

#define _MAX_FOR_DEPTH  16

static u8 no_data_hash_256[32] = { SHA256_EMPTY_HASH };
static u8 no_data_hash_1[32] = { SHA1_EMPTY_HASH };

static bool PathIsDirectory(const char* path) {
    FRESULT res;
    FILINFO fno;
    res = fvx_stat(path, &fno);
    if (res != FR_OK) return false;
    return fno.fattrib & AM_DIR;
}

static void CreateStatTable(lua_State* L, FILINFO* fno) {
    lua_createtable(L, 0, 4); // create nested table
    lua_pushstring(L, fno->fname);
    lua_setfield(L, -2, "name");
    lua_pushstring(L, (fno->fattrib & AM_DIR) ? "dir" : "file");
    lua_setfield(L, -2, "type");
    lua_pushinteger(L, fno->fsize);
    lua_setfield(L, -2, "size");
    lua_pushboolean(L, fno->fattrib & AM_RDO);
    lua_setfield(L, -2, "read_only");
    // ... and leave this table on the stack for the caller to deal with
}

static int internalfs_move(lua_State* L) {
    bool extra = CheckLuaArgCountPlusExtra(L, 2, "_fs.move");
    const char* path_src = luaL_checkstring(L, 1);
    const char* path_dst = luaL_checkstring(L, 2);
    FILINFO fno;

    CheckWritePermissionsLuaError(L, path_src);
    CheckWritePermissionsLuaError(L, path_dst);

    u32 flags = BUILD_PATH;
    if (extra) {
        flags = GetFlagsFromTable(L, 3, flags, NO_CANCEL | SILENT | OVERWRITE_ALL | SKIP_ALL);
    }

    if (!(flags & OVERWRITE_ALL) && (fvx_stat(path_dst, &fno) == FR_OK)) {
        return luaL_error(L, "destination already exists on %s -> %s and {overwrite=true} was not used", path_src, path_dst);
    }

    if (!(PathMoveCopy(path_dst, path_src, &flags, true))) {
        return luaL_error(L, "PathMoveCopy failed on %s -> %s", path_src, path_dst);
    }
    return 0;
}

static int internalfs_remove(lua_State* L) {
    bool extra = CheckLuaArgCountPlusExtra(L, 1, "_fs.remove");
    const char* path = luaL_checkstring(L, 1);

    CheckWritePermissionsLuaError(L, path);

    u32 flags = 0;
    if (extra) {
        flags = GetFlagsFromTable(L, 2, flags, RECURSIVE);
    }

    if (!(flags & RECURSIVE)) {
        if (PathIsDirectory(path)) {
            return luaL_error(L, "requested directory remove without {recursive=true} on %s", path);
        }
    }

    if (!(PathDelete(path))) {
        return luaL_error(L, "PathDelete failed on %s", path);
    }
    
    return 0;
}

static int internalfs_copy(lua_State* L) {
    bool extra = CheckLuaArgCountPlusExtra(L, 2, "_fs.copy");
    const char* path_src = luaL_checkstring(L, 1);
    const char* path_dst = luaL_checkstring(L, 2);
    FILINFO fno;

    CheckWritePermissionsLuaError(L, path_dst);

    u32 flags = BUILD_PATH;
    if (extra) {
        flags = GetFlagsFromTable(L, 3, flags, CALC_SHA | USE_SHA1 | NO_CANCEL | SILENT | OVERWRITE_ALL | SKIP_ALL | APPEND_ALL | RECURSIVE);
    }

    if (!(flags & RECURSIVE)) {
        if (PathIsDirectory(path_src)) {
            return luaL_error(L, "requested directory copy without {recursive=true} on %s -> %s", path_src, path_dst);
        }
    }

    if (!(flags & OVERWRITE_ALL) && (fvx_stat(path_dst, &fno) == FR_OK)) {
        return luaL_error(L, "destination already exists on %s -> %s and {overwrite=true} was not used", path_src, path_dst);
    }

    if (!(PathMoveCopy(path_dst, path_src, &flags, false))) {
        return luaL_error(L, "PathMoveCopy failed on %s -> %s", path_src, path_dst);
    }

    return 0;
}

static int internalfs_mkdir(lua_State* L) {
    CheckLuaArgCount(L, 1, "_fs.mkdir");
    const char* path = luaL_checkstring(L, 1);

    CheckWritePermissionsLuaError(L, path);

    FRESULT res = fvx_rmkdir(path);
    if (res != FR_OK) {
        return luaL_error(L, "could not mkdir (%d)", path, res);
    }

    return 0;
}

static int internalfs_list_dir(lua_State* L) {
    CheckLuaArgCount(L, 1, "_fs.list_dir");
    const char* path = luaL_checkstring(L, 1);
    lua_newtable(L);

    DIR dir;
    FILINFO fno;

    FRESULT res = fvx_opendir(&dir, path);
    if (res != FR_OK) {
        lua_pop(L, 1); // remove final table from stack
        return luaL_error(L, "could not opendir %s (%d)", path, res);
    }

    for (int i = 1; true; i++) {
        res = fvx_readdir(&dir, &fno);
        if (res != FR_OK) {
            lua_pop(L, 1); // remove final table from stack
            return luaL_error(L, "could not readdir %s (%d)", path, res);
        }
        if (fno.fname[0] == 0) break;
        CreateStatTable(L, &fno);
        lua_seti(L, -2, i); // add nested table to final table
    }

    return 1;
}

static int internalfs_stat(lua_State* L) {
    CheckLuaArgCount(L, 1, "_fs.stat");
    const char* path = luaL_checkstring(L, 1);
    FILINFO fno;

    FRESULT res = fvx_stat(path, &fno);
    if (res != FR_OK) {
        return luaL_error(L, "could not stat %s (%d)", path, res);
    }
    CreateStatTable(L, &fno);
    return 1;
}

// TODO: make this manually check for permissions recursively
static int internalfs_fix_cmacs(lua_State* L) {
    CheckLuaArgCount(L, 1, "_fs.fix_cmacs");
    const char* path = luaL_checkstring(L, 1);

    ShowString("%s", STR_FIXING_CMACS_PLEASE_WAIT);
    if (RecursiveFixFileCmac(path) != 0) {
        return luaL_error(L, "fixcmac failed");
    }

    return 0;
}

static int internalfs_stat_fs(lua_State* L) {
    CheckLuaArgCount(L, 1, "_fs.stat_fs");
    const char* path = luaL_checkstring(L, 1);

    u64 freespace = GetFreeSpace(path);
    u64 totalspace = GetTotalSpace(path);
    u64 usedspace = totalspace - freespace;

    lua_createtable(L, 0, 3);
    lua_pushinteger(L, freespace);
    lua_setfield(L, -2, "free");
    lua_pushinteger(L, totalspace);
    lua_setfield(L, -2, "total");
    lua_pushinteger(L, usedspace);
    lua_setfield(L, -2, "used");

    return 1;
}

static int internalfs_dir_info(lua_State* L) {
    CheckLuaArgCount(L, 1, "_fs.dir_info");
    const char* path = luaL_checkstring(L, 1);

    u64 tsize = 0;
    u32 tdirs = 0;
    u32 tfiles = 0;
    if (!DirInfo(path, &tsize, &tdirs, &tfiles)) {
        return luaL_error(L, "error when running DirInfo");
    }

    lua_createtable(L, 0, 3);
    lua_pushinteger(L, tsize);
    lua_setfield(L, -2, "size");
    lua_pushinteger(L, tdirs);
    lua_setfield(L, -2, "dirs");
    lua_pushinteger(L, tfiles);
    lua_setfield(L, -2, "files");

    return 1;
}

static int FileDirSelector(lua_State* L, const char* path_orig, const char* prompt, bool is_dir, bool include_dirs, bool explorer) {
    bool ret;
    char path[_VAR_CNT_LEN] = { 0 };
    char choice[_VAR_CNT_LEN] = { 0 };
    strncpy(path, path_orig, _VAR_CNT_LEN);
    if (strncmp(path, "Z:", 2) == 0) {
        return luaL_error(L, "forbidden drive");
    } else if (!is_dir) {
        u32 flags_ext = include_dirs ? 0 : NO_DIRS;
        char *npattern = strrchr(path, '/');
        if (!npattern) {
            return luaL_error(L, "invalid path");
        }
        *(npattern++) = '\0';
        ret = FileSelector(choice, prompt, path, npattern, flags_ext, explorer);
    } else {
        ret = FileSelector(choice, prompt, path, NULL, NO_FILES | SELECT_DIRS, explorer);
    }

    if (ret) {
        lua_pushstring(L, choice);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static int internalfs_ask_select_file(lua_State* L) {
    bool extra = CheckLuaArgCountPlusExtra(L, 2, "_fs.ask_select_file");
    const char* prompt = luaL_checkstring(L, 1);
    const char* path = luaL_checkstring(L, 2);

    u32 flags = 0;
    if (extra) {
        flags = GetFlagsFromTable(L, 3, flags, INCLUDE_DIRS | EXPLORER);
    };

    return FileDirSelector(L, path, prompt, false, (flags & INCLUDE_DIRS), (flags & EXPLORER));
}

static int internalfs_ask_select_dir(lua_State* L) {
    bool extra = CheckLuaArgCountPlusExtra(L, 2, "_fs.ask_select_dir");
    const char* prompt = luaL_checkstring(L, 1);
    const char* path = luaL_checkstring(L, 2);

    u32 flags = 0;
    if (extra) {
        flags = GetFlagsFromTable(L, 3, flags, EXPLORER);
    };

    return FileDirSelector(L, path, prompt, true, true, (flags & EXPLORER));
}

static int internalfs_find(lua_State* L) {
    bool extra = CheckLuaArgCountPlusExtra(L, 1, "_fs.find");
    const char* pattern = luaL_checkstring(L, 1);
    char path[_VAR_CNT_LEN] = { 0 };

    u32 flags = 0;
    if (extra) {
        flags = GetFlagsFromTable(L, 2, flags, FIND_FIRST);
    }

    u8 mode = (flags & FIND_FIRST) ? FN_LOWEST : FN_HIGHEST;
    FRESULT res = fvx_findpath(path, pattern, mode);
    if (res == FR_NO_PATH) {
        lua_pushnil(L);
    } else if (res != FR_OK) {
        return luaL_error(L, "failed to find %s (%d)", pattern, res);
    } else {
        lua_pushstring(L, path);
    }
    return 1;
}

// this should probably be rewritten
static int internalfs_find_all(lua_State* L) {
    bool extra = CheckLuaArgCountPlusExtra(L, 2, "_fs.find_all");
    const char* dir = luaL_checkstring(L, 1);
    const char* pattern = luaL_checkstring(L, 2);

    u32 flags = 0;
    if (extra) {
        flags = GetFlagsFromTable(L, 3, flags, RECURSIVE);
    }

    char forpath[_VAR_CNT_LEN] = { 0 };

    // without re-implementing for_handler, i need to give it a "*" pattern
    // and then manually compare each filename to see if it matches
    // so that a recursive search actually works
    bool forstatus = for_handler(NULL, dir, "*", flags & RECURSIVE);
    if (!forstatus) {
        return luaL_error(L, "could not open directory");
    }

    lua_newtable(L);
    int i = 1;
    char* slash;

    while (true) {
        forstatus = for_handler(forpath, NULL, NULL, false);
        if (!forstatus) {
            forpath[0] = '\0';
        }
        if (!forpath[0]) {
            // finish for_handler
            for_handler(NULL, NULL, NULL, false);
            break;
        } else {
            slash = strrchr(forpath, '/');
            if (!slash) bkpt; // this should never, ever happen
            if (fvx_match_name(slash+1, pattern) == FR_OK) {
                lua_pushstring(L, forpath);
                lua_seti(L, -2, i++);
            }
        }
    }

    return 1;
}

static int internalfs_find_not(lua_State* L) {
    CheckLuaArgCount(L, 1, "_fs.find_not");
    const char* pattern = luaL_checkstring(L, 1);
    char path[_VAR_CNT_LEN] = { 0 };

    FRESULT res = fvx_findnopath(path, pattern);
    if (res != FR_OK) {
        return luaL_error(L, "failed to find %s (%d)", pattern, res);
    }

    lua_pushstring(L, path);
    return 1;
}

static int internalfs_exists(lua_State* L) {
    CheckLuaArgCount(L, 1, "_fs.exists");
    const char* path = luaL_checkstring(L, 1);
    FILINFO fno;

    lua_pushboolean(L, (fvx_stat(path, &fno) == FR_OK));
    return 1;
}

static int internalfs_is_dir(lua_State* L) {
    CheckLuaArgCount(L, 1, "_fs.is_dir");
    const char* path = luaL_checkstring(L, 1);

    lua_pushboolean(L, PathIsDirectory(path));
    return 1;
}

static int internalfs_is_file(lua_State* L) {
    CheckLuaArgCount(L, 1, "_fs.is_file");
    const char* path = luaL_checkstring(L, 1);
    FILINFO fno;

    FRESULT res = fvx_stat(path, &fno);
    if (res != FR_OK) {
        lua_pushboolean(L, false);
    } else {
        lua_pushboolean(L, !(fno.fattrib & AM_DIR));
    }
    return 1;
}

static int internalfs_read_file(lua_State* L) {
    CheckLuaArgCount(L, 3, "_fs.read_file");
    const char* path = luaL_checkstring(L, 1);
    lua_Integer offset = luaL_checkinteger(L, 2);
    lua_Integer size = luaL_checkinteger(L, 3);

    char *buf = malloc(size);
    if (!buf) {
        return luaL_error(L, "could not allocate memory to read file");
    }
    UINT bytes_read = 0;
    FRESULT res = fvx_qread(path, buf, offset, size, &bytes_read);
    if (res != FR_OK) {
        free(buf);
        return luaL_error(L, "could not read %s (%d)", path, res);
    }
    lua_pushlstring(L, buf, bytes_read);
    free(buf);
    return 1;
}

static int internalfs_write_file(lua_State* L) {
    CheckLuaArgCount(L, 3, "_fs.write_file");
    const char* path = luaL_checkstring(L, 1);
    lua_Integer offset = luaL_checkinteger(L, 2);
    size_t data_length = 0;
    const char* data = luaL_checklstring(L, 3, &data_length);

    CheckWritePermissionsLuaError(L, path);
    
    UINT bytes_written = 0;
    FRESULT res = fvx_qwrite(path, data, offset, data_length, &bytes_written);
    if (res != FR_OK) {
        return luaL_error(L, "error writing %s (%d)", path, res);
    }

    lua_pushinteger(L, bytes_written);
    return 1;
}

static int internalfs_fill_file(lua_State* L) {
    bool extra = CheckLuaArgCountPlusExtra(L, 4, "_fs.fill_file");
    const char* path = luaL_checkstring(L, 1);
    lua_Integer offset = luaL_checkinteger(L, 2);
    lua_Integer size = luaL_checkinteger(L, 3);
    lua_Integer byte = luaL_checkinteger(L, 4);

    u8 real_byte = byte & 0xFF;

    u32 flags = ALLOW_EXPAND;
    if (extra) {
        flags = GetFlagsFromTable(L, 4, flags, NO_CANCEL);
    };

    if ((byte < 0) || (byte > 0xFF)) {
        return luaL_error(L, "byte is not between 0x00 and 0xFF (got: %I)", byte);
    }

    CheckWritePermissionsLuaError(L, path);

    if (!(FileSetByte(path, offset, size, real_byte, &flags))) {
        return luaL_error(L, "FileSetByte failed on %s", path);
    }

    return 0;
}

static int internalfs_make_dummy_file(lua_State* L) {
    CheckLuaArgCount(L, 2, "_fs.make_dummy_file");
    const char* path = luaL_checkstring(L, 1);
    lua_Integer size = luaL_checkinteger(L, 2);

    CheckWritePermissionsLuaError(L, path);

    if (!(FileCreateDummy(path, NULL, size))) {
        return luaL_error(L, "FileCreateDummy failed on %s");
    }

    return 0;
}

static int internalfs_truncate(lua_State* L) {
    CheckLuaArgCount(L, 2, "_fs.truncate");
    const char* path = luaL_checkstring(L, 1);
    lua_Integer size = luaL_checkinteger(L, 2);
    FIL fp;
    FRESULT res;

    CheckWritePermissionsLuaError(L, path);

    res = f_open(&fp, path, FA_READ | FA_WRITE);
    if (res != FR_OK) {
        return luaL_error(L, "failed to open %s (note: this only works on FAT filesystems, not virtual)", path);
    }

    res = f_lseek(&fp, size);
    if (res != FR_OK) {
        f_close(&fp);
        return luaL_error(L, "failed to seek on %s", path);
    }

    res = f_truncate(&fp);
    if (res != FR_OK) {
        f_close(&fp);
        return luaL_error(L, "failed to truncate %s", path);
    }

    f_close(&fp);
    return 0;
}

static int internalfs_img_mount(lua_State* L) {
    CheckLuaArgCount(L, 1, "_fs.img_mount");
    const char* path = luaL_checkstring(L, 1);

    bool res = InitImgFS(path);
    if (!res) {
        return luaL_error(L, "failed to mount %s", path);
    }

    return 0;
}

static int internalfs_img_umount(lua_State* L) {
    CheckLuaArgCount(L, 0, "_fs.img_umount");
    
    InitImgFS(NULL);
    
    return 0;
}

static int internalfs_get_img_mount(lua_State* L) {
    CheckLuaArgCount(L, 0, "_fs.get_img_mount");

    char path[256] = { 0 };
    strncpy(path, GetMountPath(), 256);
    if (path[0] == 0) {
        // since lua treats "" as true, return a nil to make if/else easier
        lua_pushnil(L);
    } else {
        lua_pushstring(L, path);
    }

    return 1;
}

// TODO: what if someone does offset != 0 but size = 0 (end of file)?
static int internalfs_hash_file(lua_State* L) {
    bool extra = CheckLuaArgCountPlusExtra(L, 3, "_fs.hash_file");
    const char* path = luaL_checkstring(L, 1);
    lua_Integer offset = luaL_checkinteger(L, 2);
    lua_Integer size = luaL_checkinteger(L, 3);
    FRESULT res;
    FILINFO fno;

    if (size == 0) {
        res = fvx_stat(path, &fno);
        if (res != FR_OK) {
            return luaL_error(L, "failed to stat %s", path);
        }

        size = fno.fsize;
    }

    u32 flags = 0;
    if (extra) {
        flags = GetFlagsFromTable(L, 4, flags, USE_SHA1);
    }

    const u8 hashlen = (flags & USE_SHA1) ? 20 : 32;
    u8 hash_fil[0x20];

    if (size == 0) {
        // shortcut by just returning the hash of empty data
        memcpy(hash_fil, (flags & USE_SHA1) ? no_data_hash_1 : no_data_hash_256, hashlen);
    } else if (!(FileGetSha(path, hash_fil, offset, size, (flags & USE_SHA1)))) {
        return luaL_error(L, "FileGetSha failed on %s", path);
    }

    lua_pushlstring(L, (char*)hash_fil, hashlen);
    return 1;
}

static int internalfs_hash_data(lua_State* L) {
    bool extra = CheckLuaArgCountPlusExtra(L, 1, "_fs.hash_data");
    size_t data_length = 0;
    const char* data = luaL_checklstring(L, 1, &data_length);

    u32 flags = 0;
    if (extra) {
        flags = GetFlagsFromTable(L, 2, flags, USE_SHA1);
    }

    const u8 hashlen = (flags & USE_SHA1) ? 20 : 32;
    u8 hash_fil[0x20];

    if (data_length == 0) {
        // shortcut by just returning the hash of empty data
        memcpy(hash_fil, (flags & USE_SHA1) ? no_data_hash_1 : no_data_hash_256, hashlen);
    } else {
        sha_quick(hash_fil, data, data_length, (flags & USE_SHA1) ? SHA1_MODE : SHA256_MODE);
    }

    lua_pushlstring(L, (char*)hash_fil, hashlen);
    return 1;
}

static int internalfs_allow(lua_State* L) {
    bool extra = CheckLuaArgCountPlusExtra(L, 1, "_fs.allow");
    const char* path = luaL_checkstring(L, 1);
    u32 flags = 0;
    bool allowed;
    if (extra) {
        flags = GetFlagsFromTable(L, 2, 0, ASK_ALL);
    }

    if (flags & ASK_ALL) {
        allowed = CheckDirWritePermissions(path);
    } else {
        allowed = CheckWritePermissions(path);
    }
    lua_pushboolean(L, allowed);
    return 1;
};

static int internalfs_verify(lua_State* L) {
    CheckLuaArgCount(L, 1, "_fs.verify");
    const char* path = luaL_checkstring(L, 1);
    bool res;

    u64 filetype = IdentifyFileType(path);
    if (filetype & IMG_NAND) res = (ValidateNandDump(path) == 0);
    else res = (VerifyGameFile(path) == 0);

    lua_pushboolean(L, res);
    return 1;
}

static int internalfs_sd_is_mounted(lua_State* L) {
    CheckLuaArgCount(L, 0, "_fs.sd_is_mounted");

    lua_pushboolean(L, CheckSDMountState());
    return 1;
}

static int internalfs_sd_switch(lua_State* L) {
    bool extra = CheckLuaArgCountPlusExtra(L, 0, "_fs.sd_switch");
    const char* message;

    if (extra) {
        message = luaL_checkstring(L, 1);
    } else {
        message = "Please switch the SD card now.";
    }

    bool ret;

    DeinitExtFS();
    if (!(ret = CheckSDMountState())) {
        return luaL_error(L, "%s", STR_SCRIPTERR_SD_NOT_MOUNTED);
    }

    u32 pad_state;
    DeinitSDCardFS();
    ShowString("%s\n \n%s", message, STR_EJECT_SD_CARD);
    while (!((pad_state = InputWait(0)) & (BUTTON_B|SD_EJECT)));
    if (pad_state & SD_EJECT) {
        ShowString("%s\n \n%s", message, STR_INSERT_SD_CARD);
        while (!((pad_state = InputWait(0)) & (BUTTON_B|SD_INSERT)));
    }
    if (pad_state & BUTTON_B) {
        return luaL_error(L, "user canceled");
    }

    InitSDCardFS();
    AutoEmuNandBase(true);
    InitExtFS();

    return 0;
}

static int internalfs_key_dump(lua_State* L) {
    bool extra = CheckLuaArgCountPlusExtra(L, 1, "_fs.key_dump");
    const char* opts[] = {SEEDINFO_NAME, TIKDB_NAME_ENC, TIKDB_NAME_DEC, NULL};
    int opt = luaL_checkoption(L, 1, NULL, opts);
    bool ret = false;

    u32 flags = 0;
    if (extra) {
        flags = GetFlagsFromTable(L, 2, 0, OVERWRITE_ALL);
    }

    if (opt == 1 || opt == 2) {
        bool tik_dec = opt == 2;
        if (flags & OVERWRITE_ALL) fvx_unlink(tik_dec ? OUTPUT_PATH "/" TIKDB_NAME_DEC : OUTPUT_PATH "/" TIKDB_NAME_ENC);
        if (BuildTitleKeyInfo(NULL, tik_dec, false) == 0) {
            ShowString(STR_BUILDING_TO_OUT_ARG, OUTPUT_PATH, opts[opt]);
            if (((BuildTitleKeyInfo("1:/dbs/ticket.db", tik_dec, false) == 0) ||
                 (BuildTitleKeyInfo("4:/dbs/ticket.db", tik_dec, false) == 0)) &&
                (BuildTitleKeyInfo(NULL, tik_dec, true) == 0))
                ret = true;
        }
    } else if (opt == 0) {
        if (flags & OVERWRITE_ALL) fvx_unlink(OUTPUT_PATH "/" SEEDINFO_NAME);
        if (BuildSeedInfo(NULL, false) == 0) {
            ShowString(STR_BUILDING_TO_OUT_ARG, OUTPUT_PATH, opts[opt]);
            if (((BuildSeedInfo("1:", false) == 0) ||
                 (BuildSeedInfo("4:", false) == 0)) &&
                (BuildSeedInfo(NULL, true) == 0))
                ret = true;
        }
    }

    if (!ret) {
        return luaL_error(L, "building %s failed", opts[opt]);
    }

    return 0;
}

static int internalfs_cart_dump(lua_State* L) {
    bool extra = CheckLuaArgCountPlusExtra(L, 2, "_fs.cart_dump");
    const char* path = luaL_checkstring(L, 1);
    u64 fsize = (u64)luaL_checkinteger(L, 2);
    bool ret = false;
    const char* errstr = "";

    u32 flags = 0;
    if (extra) {
        flags = GetFlagsFromTable(L, 3, flags, ENCRYPTED);
    }

    CartData* cdata = (CartData*) malloc(sizeof(CartData));
    u8* buf = (u8*) malloc(STD_BUFFER_SIZE);
    ret = false;
    if (!cdata || !buf) {
        errstr = "out of memory";
    } else if (InitCartRead(cdata) != 0){
        errstr = "cart init fail";
    } else {
        SetSecureAreaEncryption(flags & ENCRYPTED);
        fvx_unlink(path);
        ret = true;
        errstr = "cart dump failed or canceled";
        for (u64 p = 0; p < fsize; p += STD_BUFFER_SIZE) {
            u64 len = min((fsize - p), STD_BUFFER_SIZE);
            ShowProgress(p, fsize, path);
            if (!ShowProgress(p, fsize, path) ||
                (ReadCartBytes(buf, p, len, cdata, false) != 0) ||
                (fvx_qwrite(path, buf, p, len, NULL) != FR_OK)) {
                ret = false;
                break;
            }
        }
    }
    free(buf);
    free(cdata);

    if (!ret) {
        return luaL_error(L, "%s", errstr);
    }
    return 0;
}

static const luaL_Reg internalfs_lib[] = {
    {"move", internalfs_move},
    {"remove", internalfs_remove},
    {"copy", internalfs_copy},
    {"mkdir", internalfs_mkdir},
    {"list_dir", internalfs_list_dir},
    {"stat", internalfs_stat},
    {"stat_fs", internalfs_stat_fs},
    {"dir_info", internalfs_dir_info},
    {"ask_select_file", internalfs_ask_select_file},
    {"ask_select_dir", internalfs_ask_select_dir},
    {"find", internalfs_find},
    {"find_all", internalfs_find_all},
    {"find_not", internalfs_find_not},
    {"exists", internalfs_exists},
    {"is_dir", internalfs_is_dir},
    {"is_file", internalfs_is_file},
    {"read_file", internalfs_read_file},
    {"write_file", internalfs_write_file},
    {"fill_file", internalfs_fill_file},
    {"make_dummy_file", internalfs_make_dummy_file},
    {"truncate", internalfs_truncate},
    {"img_mount", internalfs_img_mount},
    {"img_umount", internalfs_img_umount},
    {"get_img_mount", internalfs_get_img_mount},
    {"hash_file", internalfs_hash_file},
    {"hash_data", internalfs_hash_data},
    {"verify", internalfs_verify},
    {"allow", internalfs_allow},
    {"sd_is_mounted", internalfs_sd_is_mounted},
    {"sd_switch", internalfs_sd_switch},
    {"fix_cmacs", internalfs_fix_cmacs},
    {"key_dump", internalfs_key_dump},
    {"cart_dump", internalfs_cart_dump},
    {NULL, NULL}
};

int gm9lua_open_internalfs(lua_State* L) {
    luaL_newlib(L, internalfs_lib);
    return 1;
}
#endif
