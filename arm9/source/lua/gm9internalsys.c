#ifndef NO_LUA
#include "gm9internalsys.h"
#include "bootfirm.h"
#include "fs.h"
#include "pxi.h"
#include "game.h"
#include "power.h"
#include "sha.h"
#include "nand.h"
#include "utils.h"
#include "ui.h"
#include "rtc.h"
#include "godmode.h"

#define UNUSED(x)   ((void)(x))

static int internalsys_boot(lua_State* L) {
    CheckLuaArgCount(L, 1, "_sys.boot");
    const char* path = lua_tostring(L, 1);

    u8* firm = (u8*) malloc(FIRM_MAX_SIZE);
    if (!firm) {
        return luaL_error(L, "out of memory");
    }

    size_t firm_size = FileGetData(path, firm, FIRM_MAX_SIZE, 0);
    if (!(firm_size && IsBootableFirm(firm, firm_size))) {
        return luaL_error(L, "not a bootable firm");
    }

    char fixpath[256] = { 0 };
    if ((*path == '0') || (*path == '1'))
        snprintf(fixpath, sizeof(fixpath), "%s%s", (*path == '0') ? "sdmc" : "nand", path + 1);
    else strncpy(fixpath, path, 256);
    fixpath[255] = '\0';
    DeinitExtFS();
    DeinitSDCardFS();
    PXI_DoCMD(PXICMD_LEGACY_BOOT, NULL, 0);
    PXI_Barrier(PXI_FIRMLAUNCH_BARRIER);
    BootFirm((FirmHeader*)(void*)firm, fixpath);
    while(1);
}

static int internalsys_reboot(lua_State* L) {
    CheckLuaArgCount(L, 0, "_sys.reboot");
    DeinitExtFS();
    DeinitSDCardFS();
    Reboot();
    return 0;
}

static int internalsys_power_off(lua_State* L) {
    CheckLuaArgCount(L, 0, "_sys.power_off");
    DeinitExtFS();
    DeinitSDCardFS();
    PowerOff();
    return 0;
}

static int internalsys_get_id0(lua_State* L) {
    CheckLuaArgCount(L, 1, "_sys.get_id0");
    const char* path = lua_tostring(L, 1);

    char env_id0[32+1];
    u8 sd_keyy[0x10] __attribute__((aligned(4)));
    if (FileGetData(path, sd_keyy, 0x10, 0x110) == 0x10) {
        u32 sha256sum[8];
        sha_quick(sha256sum, sd_keyy, 0x10, SHA256_MODE);
        snprintf(env_id0, sizeof(env_id0), "%08lx%08lx%08lx%08lx",
            sha256sum[0], sha256sum[1], sha256sum[2], sha256sum[3]);
        lua_pushstring(L, env_id0);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static int internalsys_next_emu(lua_State* L) {
    CheckLuaArgCount(L, 0, "_sys.next_emu");

    DismountDriveType(DRV_EMUNAND);
    AutoEmuNandBase(false);
    InitExtFS();

    return 0;
}

static int internalsys_get_emu_base(lua_State* L) {
    CheckLuaArgCount(L, 0, "_sys.get_emu_base");

    lua_pushinteger(L, GetEmuNandBase());
    return 1;
}

static int internalsys_check_embedded_backup(lua_State* L) {
    CheckLuaArgCount(L, 0, "_sys.check_embedded_backup");

    if (PathExist("S:/essential.exefs")) {
        lua_pushboolean(L, true);
        return 1;
    }

    bool ncsd_check = CheckGenuineNandNcsd();
    if (!ncsd_check) {
        lua_pushnil(L);
        return 1;
    }

    bool ret = false;

    if (ncsd_check && ShowPrompt(true, "%s", STR_ESSENTIAL_BACKUP_NOT_FOUND_CREATE_NOW)) {
        if (EmbedEssentialBackup("S:/nand.bin") == 0) {
            u32 flags = BUILD_PATH | SKIP_ALL;
            PathCopy(OUTPUT_PATH, "S:/essential.exefs", &flags);
            ShowPrompt(false, STR_BACKUP_EMBEDDED_WRITTEN_TO_OUT, OUTPUT_PATH);
            ret = true;
        } else {
            ret = false;
        }
    } else {
        ret = false;
    }

    lua_pushboolean(L, ret);
    return 1;
}

static int internalsys_check_raw_rtc(lua_State* L) {
    CheckLuaArgCount(L, 0, "_sys.check_raw_rtc");

    bool result = false;

    DsTime dstime;
    get_dstime(&dstime);
    if (DSTIMEGET(&dstime, bcd_Y) >= 18) {
        result = true;
    } else if (ShowPrompt(true, "%s", STR_RTC_DATE_TIME_SEEMS_TO_BE_WRONG_SET_NOW) &&
         ShowRtcSetterPrompt(&dstime, "%s", STR_TITLE_SET_RTC_DATE_TIME)) {
        //char timestr[UTF_BUFFER_BYTESIZE(32)];
        set_dstime(&dstime);
        // this is only in godmode.h
        //GetTimeString(timestr, true, true);
        // ShowPrompt(false, STR_NEW_RTC_DATE_TIME_IS_TIME, timestr);
        result = true;
    }

    lua_pushboolean(L, result);
    return 1;
}

static int internalsys_global_bkpt(lua_State* L) {
    UNUSED(L);
    bkpt;
    while(1);
}

static const luaL_Reg internalsys_lib[] = {
    {"boot", internalsys_boot},
    {"reboot", internalsys_reboot},
    {"power_off", internalsys_power_off},
    {"get_id0", internalsys_get_id0},
    {"next_emu", internalsys_next_emu},
    {"get_emu_base", internalsys_get_emu_base},
    {"check_embedded_backup", internalsys_check_embedded_backup},
    {"check_raw_rtc", internalsys_check_raw_rtc},
    {NULL, NULL}
};

static const luaL_Reg internalsys_global_lib[] = {
    {"bkpt", internalsys_global_bkpt},
    {NULL, NULL}
};

int gm9lua_open_internalsys(lua_State* L) {
    luaL_newlib(L, internalsys_lib);
    lua_pushglobaltable(L); // push global table to stack
    luaL_setfuncs(L, internalsys_global_lib, 0); // set global funcs
    lua_pop(L, 1); // pop global table from stack
    return 1;
}
#endif
