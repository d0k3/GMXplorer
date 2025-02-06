#include "godmode.h"
#include "paint9.h"
#include "memmap.h"
#include "support.h"
#include "ui.h"
#include "swkbd.h"
#include "hid.h"
#include "swkbd.h"
#include "touchcal.h"
#include "fs.h"
#include "utils.h"
#include "nand.h"
#include "gamecart.h"
#include "virtual.h"
#include "vcart.h"
#include "game.h"
#include "disadiff.h"
#include "unittype.h"
#include "entrypoints.h"
#include "bootfirm.h"
#include "png.h"
#include "timer.h"
#include "rtc.h"
#include "power.h"
#include "vram0.h"
#include "i2c.h"
#include "pxi.h"
#include "language.h"
#include "gm9lua.h"

#ifndef N_PANES
#define N_PANES 3
#endif

#define COLOR_TOP_BAR   (PERM_RED ? COLOR_RED : PERM_ORANGE ? COLOR_ORANGE : PERM_BLUE ? COLOR_BRIGHTBLUE : \
                         PERM_YELLOW ? COLOR_BRIGHTYELLOW : PERM_GREEN ? COLOR_GREEN : COLOR_WHITE)

#define BOOTPAUSE_KEY   (BUTTON_R1|BUTTON_UP)
#define BOOTMENU_KEY    (BUTTON_R1|BUTTON_LEFT)
#define BOOTFIRM_PATHS  "0:/bootonce.firm", "0:/boot.firm", "1:/boot.firm"
#define BOOTFIRM_TEMPS  0x1 // bits mark paths as temporary

#ifdef SALTMODE // ShadowHand's own bootmenu key override
#undef  BOOTMENU_KEY
#define BOOTMENU_KEY    BUTTON_START
#endif


typedef struct {
    char path[256];
    u32 cursor;
    u32 scroll;
} PaneData;


u32 BootFirmHandler(const char* bootpath, bool verbose, bool delete) {
    char pathstr[UTF_BUFFER_BYTESIZE(32)];
    TruncateString(pathstr, bootpath, 32, 8);

    size_t firm_size = FileGetSize(bootpath);
    if (!firm_size) return 1;
    if (firm_size > FIRM_MAX_SIZE) {
        if (verbose) ShowPrompt(false, "%s\n%s", pathstr, STR_FIRM_TOO_BIG); // unlikely
        return 1;
    }

    if (verbose && !ShowPrompt(true, STR_PATH_DO_NOT_BOOT_UNTRUSTED, pathstr, firm_size / 1024))
        return 1;

    void* firm = (void*) malloc(FIRM_MAX_SIZE);
    if (!firm) return 1;
    if ((FileGetData(bootpath, firm, firm_size, 0) != firm_size) ||
        !IsBootableFirm(firm, firm_size)) {
        if (verbose) ShowPrompt(false, "%s\n%s", pathstr, STR_NOT_BOOTABLE_FIRM);
        free(firm);
        return 1;
    }

    // encrypted firm handling
    FirmSectionHeader* arm9s = FindFirmArm9Section(firm);
    if (!arm9s) return 1;

    FirmA9LHeader* a9l = (FirmA9LHeader*)(void*) ((u8*) firm + arm9s->offset);
    if (verbose && (ValidateFirmA9LHeader(a9l) == 0) &&
        ShowPrompt(true, "%s\n%s", pathstr, STR_FIRM_ENCRYPTED) &&
        (DecryptFirmFull(firm, firm_size) != 0)) {
        free(firm);
        return 1;
    }

    // unsupported location handling
    char fixpath[256] = { 0 };
    if (verbose && (*bootpath != '0') && (*bootpath != '1')) {
        char str[UTF_BUFFER_BYTESIZE(256)];
        snprintf(str, sizeof(str), STR_MAKE_COPY_AT_OUT_TEMP_FIRM, OUTPUT_PATH);
        const char* optionstr[2] = { str, STR_TRY_BOOT_ANYWAYS };
        u32 user_select = ShowSelectPrompt(2, optionstr, "%s\n%s", pathstr, STR_WARNING_BOOT_UNSUPPORTED_LOCATION);
        if (user_select == 1) {
            FileSetData(OUTPUT_PATH "/temp.firm", firm, firm_size, 0, true);
            bootpath = OUTPUT_PATH "/temp.firm";
        } else if (!user_select) bootpath = "";
    }

    // fix the boot path ("sdmc"/"nand" for Luma et al, hacky af)
    if ((*bootpath == '0') || (*bootpath == '1'))
        snprintf(fixpath, sizeof(fixpath), "%s%s", (*bootpath == '0') ? "sdmc" : "nand", bootpath + 1);
    else strncpy(fixpath, bootpath, 256);
    fixpath[255] = '\0';

    // boot the FIRM (if we got a proper fixpath)
    if (*fixpath) {
        if (delete) PathDelete(bootpath);
        DeinitExtFS();
        DeinitSDCardFS();
        PXI_DoCMD(PXICMD_LEGACY_BOOT, NULL, 0);
        PXI_Barrier(PXI_FIRMLAUNCH_BARRIER);
        BootFirm((FirmHeader*) firm, fixpath);
        while(1);
    }

    // a return was not intended
    free(firm);
    return 1;
}

u32 SplashInit(const char* modestr) {
    u64 splash_size;
    u8* splash = FindVTarFileInfo(VRAM0_SPLASH_PNG, &splash_size);
    const char* namestr = FLAVOR " " VERSION;
    const char* loadstr = "booting...";
    const u32 pos_xb = 10;
    const u32 pos_yb = 10;
    const u32 pos_xu = SCREEN_WIDTH_BOT - 10 - GetDrawStringWidth(loadstr);
    const u32 pos_yu = SCREEN_HEIGHT - 10 - GetDrawStringHeight(loadstr);

    ClearScreenF(true, true, COLOR_STD_BG);

    if (splash) {
        u32 splash_width, splash_height;
        u16* bitmap = PNG_Decompress(splash, splash_size, &splash_width, &splash_height);
        if (bitmap) {
            DrawBitmap(TOP_SCREEN, -1, -1, splash_width, splash_height, bitmap);
            free(bitmap);
        }
    } else {
        DrawStringF(TOP_SCREEN, 10, 10, COLOR_STD_FONT, COLOR_TRANSPARENT, "(" VRAM0_SPLASH_PNG " not found)");
    }

    if (modestr) DrawStringF(TOP_SCREEN, SCREEN_WIDTH_TOP - 10 - GetDrawStringWidth(modestr),
        SCREEN_HEIGHT - 10 - GetDrawStringHeight(modestr), COLOR_STD_FONT, COLOR_TRANSPARENT, "%s", modestr);

    DrawStringF(BOT_SCREEN, pos_xb, pos_yb, COLOR_STD_FONT, COLOR_STD_BG, "%s\n%*.*s\n%s\n \n \n%s\n%s\n \n%s\n%s",
        namestr, strlen(namestr), strlen(namestr),
        "--------------------------------", "https://github.com/d0k3/GodMode9",
        "Releases:", "https://github.com/d0k3/GodMode9/releases/", // this won't fit with a 8px width font
        "Hourlies:", "https://d0k3.secretalgorithm.com/");
    DrawStringF(BOT_SCREEN, pos_xu, pos_yu, COLOR_STD_FONT, COLOR_STD_BG, "%s", loadstr);
    DrawStringF(BOT_SCREEN, pos_xb, pos_yu, COLOR_STD_FONT, COLOR_STD_BG, "built: " DBUILTL);

    return 0;
}

#ifndef SCRIPT_RUNNER
static DirStruct* current_dir = NULL;
static DirStruct* clipboard   = NULL;
static PaneData* panedata     = NULL;

void GetTimeString(char* timestr, bool forced_update, bool full_year) { // timestr should be 32 bytes
    static DsTime dstime;
    static u64 timer = (u64) -1; // this ensures we don't check the time too often
    if (forced_update || (timer == (u64) -1) || (timer_sec(timer) > 30)) {
        get_dstime(&dstime);
        timer = timer_start();
    }
    if (timestr) snprintf(timestr, UTF_BUFFER_BYTESIZE(32), STR_DATE_TIME_FORMAT, full_year ? "20" : "",
        (u32) dstime.bcd_Y, (u32) dstime.bcd_M, (u32) dstime.bcd_D, (u32) dstime.bcd_h, (u32) dstime.bcd_m);
}

void CheckBattery(u32* battery, bool* is_charging) {
    if (battery) {
        static u32 battery_l = 0;
        static u64 timer_b = (u64) -1; // this ensures we don't check too often
        if ((timer_b == (u64) -1) || (timer_sec(timer_b) >= 120)) {
            battery_l = GetBatteryPercent();
            timer_b = timer_start();
        }
        *battery = battery_l;
    }

    if (is_charging) {
        static bool is_charging_l = false;
        static u64 timer_c = (u64) -1;
        if ((timer_c == (u64) -1) || (timer_sec(timer_c) >= 1)) {
            is_charging_l = IsCharging();
            timer_c = timer_start();
        }
        *is_charging = is_charging_l;
    }
}

void DrawBatteryBitmap(u16* screen, u32 b_x, u32 b_y, u32 width, u32 height) {
    const u16 color_outline = COLOR_BLACK;
    const u16 color_inline = COLOR_LIGHTGREY;
    const u16 color_inside = COLOR_LIGHTERGREY;
    const u16 color_bg = COLOR_TRANSPARENT;

    if ((width < 8) || (height < 6)) return;

    u32 battery;
    bool is_charging;
    CheckBattery(&battery, &is_charging);

    u16 color_battery = (is_charging) ? COLOR_BATTERY_CHARGING :
        (battery > 70) ? COLOR_BATTERY_FULL : (battery > 30) ? COLOR_BATTERY_MEDIUM : COLOR_BATTERY_LOW;
    u32 nub_size = (height < 12) ? 1 : 2;
    u32 width_inside = width - 4 - nub_size;
    u32 width_battery = (battery >= 100) ? width_inside : ((battery * width_inside) + 50) / 100;

    for (u32 y = 0; y < height; y++) {
        const u32 mirror_y = (y >= (height+1) / 2) ? height - 1 - y : y;
        for (u32 x = 0; x < width; x++) {
            const u32 rev_x = width - x - 1;
            u16 color = 0;
            if (mirror_y == 0) color = (rev_x >= nub_size) ? color_outline : color_bg;
            else if (mirror_y == 1) color = ((x == 0) || (rev_x == nub_size)) ? color_outline : (rev_x < nub_size) ? color_bg : color_inline;
            else if (mirror_y == 2) color = ((x == 0) || (rev_x <= nub_size)) ? color_outline : ((x == 1) || (rev_x == (nub_size+1))) ? color_inline : color_inside;
            else color = ((x == 0) || (rev_x == 0)) ? color_outline : ((x == 1) || (rev_x <= (nub_size+1))) ? color_inline : color_inside;
            if ((color == color_inside) && (x < (2 + width_battery))) color = color_battery;
            if (color != color_bg) DrawPixel(screen, b_x + x, b_y + y, color);
        }
    }
}

void DrawTopBar(const char* curr_path) {
    const u32 bartxt_start = (FONT_HEIGHT_EXT >= 10) ? 1 : (FONT_HEIGHT_EXT >= 7) ? 2 : 3;
    const u32 bartxt_x = 2;
    const u32 len_path = SCREEN_WIDTH_TOP - 120;
    char tempstr[UTF_BUFFER_BYTESIZE(63)];

    // top bar - current path
    DrawRectangle(TOP_SCREEN, 0, 0, SCREEN_WIDTH_TOP, 12, COLOR_TOP_BAR);
    if (*curr_path) TruncateString(tempstr, curr_path, min(63, len_path / FONT_WIDTH_EXT), 8);
    else snprintf(tempstr, sizeof(tempstr), "%s", STR_ROOT);
    DrawStringF(TOP_SCREEN, bartxt_x, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, "%s", tempstr);
    bool show_time = true;

    #ifdef SHOW_FREE
    if (*curr_path) { // free & total storage
        const u32 bartxt_rx = SCREEN_WIDTH_TOP - (19*FONT_WIDTH_EXT) - bartxt_x;
        char bytestr0[32];
        char bytestr1[32];
        char tempstr[UTF_BUFFER_BYTESIZE(19)];
        ResizeString(tempstr, STR_LOADING, 19, 19, true);
        DrawString(TOP_SCREEN, tempstr, bartxt_rx, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR);
        FormatBytes(bytestr0, GetFreeSpace(curr_path));
        FormatBytes(bytestr1, GetTotalSpace(curr_path));
        snprintf(tempstr, sizeof(tempstr), "%s/%s", bytestr0, bytestr1);
        DrawStringF(TOP_SCREEN, bartxt_rx, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, "%19.19s", tempstr);
        show_time = false;
    }
    #elif defined MONITOR_HEAP
    if (true) { // allocated mem
        const u32 bartxt_rx = SCREEN_WIDTH_TOP - (9*FONT_WIDTH_EXT) - bartxt_x;
        char bytestr[32];
        FormatBytes(bytestr, mem_allocated());
        DrawStringF(TOP_SCREEN, bartxt_rx, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, "%9.9s", bytestr);
        show_time = false;
    }
    #endif

    if (show_time) { // clock & battery
        char timestr[UTF_BUFFER_BYTESIZE(32)];
        GetTimeString(timestr, false, false);

        const u32 battery_width = 16;
        const u32 battery_height = 9;
        const u32 battery_x = SCREEN_WIDTH_TOP - battery_width - bartxt_x;
        const u32 battery_y = (12 - battery_height) / 2;
        const u32 clock_x = battery_x - (GetDrawStringWidth(timestr) + FONT_WIDTH_EXT);

        DrawStringF(TOP_SCREEN, clock_x, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, "%s", timestr);
        DrawBatteryBitmap(TOP_SCREEN, battery_x, battery_y, battery_width, battery_height);
    }
}

void DrawUserInterface(const char* curr_path, DirEntry* curr_entry, u32 curr_pane) {
    const u32 n_cb_show = 8;
    const u32 info_start = (MAIN_SCREEN == TOP_SCREEN) ? 18 : 2; // leave space for the topbar when required
    const u32 instr_x = (SCREEN_WIDTH_MAIN - (34*FONT_WIDTH_EXT)) / 2;
    const u32 len_info = (SCREEN_WIDTH_MAIN - ((SCREEN_WIDTH_MAIN >= 400) ? 80 : 20)) / 2;
    const u32 str_len_info = min(63, len_info / FONT_WIDTH_EXT);
    char tempstr[UTF_BUFFER_BYTESIZE(str_len_info)];

    static u32 state_prev = 0xFFFFFFFF;
    u32 state_curr =
        ((*curr_path) ? (1<<0) : 0) |
        ((clipboard->n_entries) ? (1<<1) : 0) |
        ((CheckSDMountState()) ? (1<<2) : 0) |
        ((GetMountState()) ? (1<<3) : 0) |
        ((GetWritePermissions() > PERM_BASE) ? (1<<4) : 0) |
        (curr_pane<<5);

    if (state_prev != state_curr) {
        ClearScreenF(true, false, COLOR_STD_BG);
        state_prev = state_curr;
    }

    // left top - current file info
    if (curr_pane) snprintf(tempstr, sizeof(tempstr), STR_PANE_N, curr_pane);
    else snprintf(tempstr, sizeof(tempstr), "%s", STR_CURRENT);
    DrawStringF(MAIN_SCREEN, 2, info_start, COLOR_STD_FONT, COLOR_STD_BG, "[%s]", tempstr);
    // file / entry name
    ResizeString(tempstr, curr_entry->name, str_len_info, 8, false);
    u32 color_current = COLOR_ENTRY(curr_entry);
    DrawStringF(MAIN_SCREEN, 4, info_start + 12, color_current, COLOR_STD_BG, "%s", tempstr);
    // size (in Byte) or type desc
    if (curr_entry->type == T_DIR) {
        ResizeString(tempstr, STR_DIR, str_len_info, 8, false);
    } else if (curr_entry->type == T_DOTDOT) {
        snprintf(tempstr, sizeof(tempstr), "%20s", "");
    } else if (curr_entry->type == T_ROOT) {
        int drvtype = DriveType(curr_entry->path);
        const char* drvstr =
            (drvtype & DRV_SDCARD) ? STR_SD_FAT : (drvtype & DRV_RAMDRIVE) ? STR_RAMDRIVE_FAT : (drvtype & DRV_GAME) ? STR_GAME_VIRTUAL :
            (drvtype & (DRV_SYSNAND | DRV_FAT)) ? STR_SYSNAND_FAT : (drvtype & (DRV_SYSNAND | DRV_VIRTUAL)) ? STR_SYSNAND_VIRTUAL :
            (drvtype & (DRV_EMUNAND | DRV_FAT)) ? STR_EMUNAND_FAT : (drvtype & (DRV_EMUNAND | DRV_VIRTUAL)) ? STR_EMUNAND_VIRTUAL :
            (drvtype & DRV_IMAGE) ? STR_IMAGE_FAT : (drvtype & DRV_XORPAD) ? STR_XORPAD_VIRTUAL : (drvtype & DRV_MEMORY) ? STR_MEMORY_VIRTUAL :
            (drvtype & DRV_ALIAS) ? STR_ALIAS_FAT : (drvtype & DRV_CART) ? STR_GAMECART_VIRTUAL : (drvtype & DRV_VRAM) ? STR_VRAM_VIRTUAL :
            (drvtype & DRV_SEARCH) ? STR_SEARCH : (drvtype & DRV_TITLEMAN) ? STR_TITLEMANAGER_VIRTUAL : "";
        ResizeString(tempstr, drvstr, str_len_info, 8, false);
    } else {
        char numstr[UTF_BUFFER_BYTESIZE(32)];
        char bytestr[UTF_BUFFER_BYTESIZE(32)];
        FormatNumber(numstr, curr_entry->size);
        snprintf(bytestr, sizeof(bytestr), STR_N_BYTE, numstr);
        ResizeString(tempstr, bytestr, str_len_info, 8, false);
    }
    DrawStringF(MAIN_SCREEN, 4, info_start + 12 + 10, color_current, COLOR_STD_BG, "%s", tempstr);
    // path of file (if in search results)
    if ((DriveType(curr_path) & DRV_SEARCH) && strrchr(curr_entry->path, '/')) {
        char dirstr[256];
        strncpy(dirstr, curr_entry->path, 256);
        *(strrchr(dirstr, '/')+1) = '\0';
        ResizeString(tempstr, dirstr, str_len_info, 8, false);
        DrawStringF(MAIN_SCREEN, 4, info_start + 12 + 10 + 10, color_current, COLOR_STD_BG, "%s", tempstr);
    } else {
        ResizeString(tempstr, "", str_len_info, 8, false);
        DrawStringF(MAIN_SCREEN, 4, info_start + 12 + 10 + 10, color_current, COLOR_STD_BG, "%s", tempstr);
    }

    // right top - clipboard
    DrawStringF(MAIN_SCREEN, SCREEN_WIDTH_MAIN - len_info, info_start, COLOR_STD_FONT, COLOR_STD_BG, "%*s",
        (int) (len_info / FONT_WIDTH_EXT), (clipboard->n_entries) ? STR_CLIPBOARD : "");
    for (u32 c = 0; c < n_cb_show; c++) {
        u32 color_cb = COLOR_ENTRY(&(clipboard->entry[c]));
        ResizeString(tempstr, (clipboard->n_entries > c) ? clipboard->entry[c].name : "", str_len_info, 8, true);
        DrawStringF(MAIN_SCREEN, SCREEN_WIDTH_MAIN - len_info - 4, info_start + 12 + (c*10), color_cb, COLOR_STD_BG, "%s", tempstr);
    }
    *tempstr = '\0';
    if (clipboard->n_entries > n_cb_show) snprintf(tempstr, sizeof(tempstr), STR_PLUS_N_MORE, clipboard->n_entries - n_cb_show);
    DrawStringF(MAIN_SCREEN, SCREEN_WIDTH_MAIN - len_info - 4, info_start + 12 + (n_cb_show*10), COLOR_DARKGREY, COLOR_STD_BG,
        "%*s", (int) (len_info / FONT_WIDTH_EXT), tempstr);

    // bottom: instruction block
    char instr[UTF_BUFFER_BYTESIZE(512)];
    snprintf(instr, sizeof(instr), "%s\n%s%s%s%s%s%s%s%s",
        FLAVOR " " VERSION, // generic start part
        (*curr_path) ? ((clipboard->n_entries == 0) ? STR_MARK_DELETE_COPY : STR_MARK_DELETE_PASTE) :
        ((GetWritePermissions() > PERM_BASE) ? STR_RELOCK_WRITE_PERMISSION : ""),
        (*curr_path) ? "" : (GetMountState()) ? STR_UNMOUNT_IMAGE : "",
        (*curr_path) ? "" : (CheckSDMountState()) ? STR_UNMOUNT_SD : STR_REMOUNT_SD,
        (*curr_path) ? STR_DIRECTORY_OPTIONS : STR_DRIVE_OPTIONS,
        STR_MAKE_SCREENSHOT,
        STR_PREV_NEXT_PANE,
        (clipboard->n_entries) ? STR_CLEAR_CLIPBOARD : STR_RESTORE_CLIPBOARD, // only if clipboard is full
        STR_REBOOT_POWEROFF_HOME); // generic end part
    DrawStringF(MAIN_SCREEN, instr_x, SCREEN_HEIGHT - 4 - GetDrawStringHeight(instr), COLOR_STD_FONT, COLOR_STD_BG, "%s", instr);
}

void DrawDirContents(DirStruct* contents, u32 cursor, u32* scroll) {
    const int str_width = (SCREEN_WIDTH_ALT-3) / FONT_WIDTH_EXT;
    const u32 stp_y = min(12, FONT_HEIGHT_EXT + 4);
    const u32 start_y = (MAIN_SCREEN == TOP_SCREEN) ? 0 : 12;
    const u32 pos_x = 0;
    const u32 lines = (SCREEN_HEIGHT-(start_y+2)+(stp_y-1)) / stp_y;
    u32 pos_y = start_y + 2;

    if (*scroll > cursor) *scroll = cursor;
    else if (*scroll + lines <= cursor) *scroll = cursor - lines + 1;
    if (*scroll + lines > contents->n_entries)
        *scroll = (contents->n_entries > lines) ? contents->n_entries - lines : 0;

    for (u32 i = 0; pos_y < SCREEN_HEIGHT; i++) {
        char tempstr[UTF_BUFFER_BYTESIZE(str_width)];
        u32 offset_i = *scroll + i;
        u32 color_font = COLOR_WHITE;
        if (offset_i < contents->n_entries) {
            DirEntry* curr_entry = &(contents->entry[offset_i]);
            char namestr[UTF_BUFFER_BYTESIZE(str_width - 10)];
            char rawbytestr[32], bytestr[UTF_BUFFER_BYTESIZE(10)];
            color_font = (cursor != offset_i) ? COLOR_ENTRY(curr_entry) : COLOR_STD_FONT;
            FormatBytes(rawbytestr, curr_entry->size);
            ResizeString(bytestr, (curr_entry->type == T_DIR) ? STR_DIR : (curr_entry->type == T_DOTDOT) ? "(..)" : rawbytestr, 10, 10, true);
            ResizeString(namestr, curr_entry->name, str_width - 10, str_width - 20, false);
            snprintf(tempstr, sizeof(tempstr), "%s%s", namestr, bytestr);
        } else snprintf(tempstr, sizeof(tempstr), "%-*.*s", str_width, str_width, "");
        DrawString(ALT_SCREEN, tempstr, pos_x, pos_y, color_font, COLOR_STD_BG);
        pos_y += stp_y;
    }

    const u32 flist_height = (SCREEN_HEIGHT - start_y);
    const u32 bar_width = 2;
    if (contents->n_entries > lines) { // draw position bar at the right
        const u32 bar_height_min = 32;
        u32 bar_height = (lines * flist_height) / contents->n_entries;
        if (bar_height < bar_height_min) bar_height = bar_height_min;
        const u32 bar_pos = ((u64) *scroll * (flist_height - bar_height)) / (contents->n_entries - lines) + start_y;

        DrawRectangle(ALT_SCREEN, SCREEN_WIDTH_ALT - bar_width, start_y, bar_width, (bar_pos - start_y), COLOR_STD_BG);
        DrawRectangle(ALT_SCREEN, SCREEN_WIDTH_ALT - bar_width, bar_pos + bar_height, bar_width, SCREEN_HEIGHT - (bar_pos + bar_height), COLOR_STD_BG);
        DrawRectangle(ALT_SCREEN, SCREEN_WIDTH_ALT - bar_width, bar_pos, bar_width, bar_height, COLOR_SIDE_BAR);
    } else DrawRectangle(ALT_SCREEN, SCREEN_WIDTH_ALT - bar_width, start_y, bar_width, flist_height, COLOR_STD_BG);
}

u32 SdFormatMenu(const char* slabel) {
    static const u32 cluster_size_table[5] = { 0x0, 0x0, 0x4000, 0x8000, 0x10000 };
    const char* option_emunand_size[7] = { STR_NO_EMUNAND, STR_REDNAND_SIZE_MIN, STR_GW_EMUNAND_SIZE_FULL,
        STR_MULTINAND_SIZE_2X, STR_MULTINAND_SIZE_3X, STR_MULTINAND_SIZE_4X, STR_USER_INPUT };
    const char* option_cluster_size[4] = { STR_AUTO, STR_16KB_CLUSTERS, STR_32KB_CLUSTERS, STR_64KB_CLUSTERS };
    u32 sysnand_min_size_sectors = GetNandMinSizeSectors(NAND_SYSNAND);
    u64 sysnand_min_size_mb = ((sysnand_min_size_sectors * 0x200) + 0xFFFFF) / 0x100000;
    u64 sysnand_multi_size_mb = (align(sysnand_min_size_sectors + 1, 0x2000) * 0x200) / 0x100000;
    u64 sysnand_size_mb = (((u64)GetNandSizeSectors(NAND_SYSNAND) * 0x200) + 0xFFFFF) / 0x100000;
    char label[DRV_LABEL_LEN + 4];
    u32 cluster_size = 0;
    u64 sdcard_size_mb = 0;
    u64 emunand_size_mb = (u64) -1;
    u32 user_select = 0;

    // check actual SD card size
    sdcard_size_mb = GetSDCardSize() / 0x100000;
    if (!sdcard_size_mb) {
        ShowPrompt(false, "%s", STR_SD_NOT_DETECTED);
        return 1;
    }

    user_select = ShowSelectPrompt(7, option_emunand_size, STR_FORMAT_SD_CHOOSE_EMUNAND, sdcard_size_mb);
    if (user_select && (user_select < 4)) {
        emunand_size_mb = (user_select == 2) ? sysnand_min_size_mb : (user_select == 3) ? sysnand_size_mb : 0;
    } else if ((user_select >= 4) && (user_select <= 6)) {
        u32 n = (user_select - 2);
        emunand_size_mb = n * sysnand_multi_size_mb;
    } else if (user_select == 7) do {
        emunand_size_mb = ShowNumberPrompt(sysnand_min_size_mb, STR_SD_SIZE_IS_ENTER_EMUNAND_SIZE, sdcard_size_mb);
        if (emunand_size_mb == (u64) -1) break;
    } while (emunand_size_mb > sdcard_size_mb);
    if (emunand_size_mb == (u64) -1) return 1;

    user_select = ShowSelectPrompt(4, option_cluster_size, STR_FORMAT_SD_CHOOSE_CLUSTER, sdcard_size_mb);
    if (!user_select) return 1;
    else cluster_size = cluster_size_table[user_select];

    snprintf(label, sizeof(label), "0:%s", (slabel && *slabel) ? slabel : "GM9SD");
    if (!ShowKeyboardOrPrompt(label + 2, 11 + 1, STR_FORMAT_SD_ENTER_LABEL, sdcard_size_mb))
        return 1;

    if (!FormatSDCard(emunand_size_mb, cluster_size, label)) {
        ShowPrompt(false, "%s", STR_FORMAT_SD_FAILED);
        return 1;
    }

    if (emunand_size_mb >= sysnand_min_size_mb) {
        u32 emunand_offset = 1;
        u32 n_emunands = 1;
        if (emunand_size_mb >= 2 * sysnand_multi_size_mb) { /* space can fit single/multi RedNAND or GW type */
            const char* option_emunand_type[4] = { STR_REDNAND_TYPE_MULTI, STR_REDNAND_TYPE_SINGLE, STR_DONT_SET_UP, 0 };
            u32 n_opt = 3;
            if (emunand_size_mb >= sysnand_size_mb) {
                option_emunand_type[2] = STR_GW_EMUNAND_TYPE;
                option_emunand_type[3] = STR_DONT_SET_UP;
                n_opt++;
            }
            user_select = ShowSelectPrompt(n_opt, option_emunand_type, "%s", STR_CHOOSE_EMUNAND_TYPE);
            if (user_select == n_opt) return 0;
            emunand_offset = (n_opt == 4 && user_select == 3) ? 0 : 1;
            if (user_select == 1) n_emunands = 4;
        } else if (emunand_size_mb >= sysnand_size_mb) { /* space can fit single RedNAND or GW type, but not multi RedNAND */
            const char* option_emunand_type[3] = { STR_REDNAND_TYPE, STR_GW_EMUNAND_TYPE, STR_DONT_SET_UP };
            user_select = ShowSelectPrompt(3, option_emunand_type, "%s", STR_CHOOSE_EMUNAND_TYPE);
            if (user_select > 2) return 0;
            emunand_offset = (user_select == 2) ? 0 : 1; // 0 -> GW EmuNAND
        } else user_select = ShowPrompt(true, "%s", STR_CLONE_SYSNAND_TO_REDNAND) ? 1 : 0; /* space can only fit a single RedNAND */
        if (!user_select) return 0;

        u8 ncsd[0x200];
        u32 flags = OVERRIDE_PERM;
        InitSDCardFS(); // this has to be initialized for EmuNAND to work
        for (u32 i = 0; i < n_emunands; i++) {
            if ((i * sysnand_multi_size_mb) + sysnand_min_size_mb > emunand_size_mb) break;
            SetEmuNandBase((i * sysnand_multi_size_mb * 0x100000 / 0x200) + emunand_offset);
            if ((ReadNandSectors(ncsd, 0, 1, 0xFF, NAND_SYSNAND) != 0) ||
                (WriteNandSectors(ncsd, 0, 1, 0xFF, NAND_EMUNAND) != 0) ||
                (!PathCopy("E:", "S:/nand_minsize.bin", &flags))) {
                ShowPrompt(false, "%s", STR_CLONING_SYSNAND_TO_EMUNAND_FAILED);
                break;
            }
        }
        DeinitSDCardFS();
    }

    return 0;
}

u32 FileGraphicsViewer(const char* path) {
    const u32 max_size = SCREEN_SIZE(ALT_SCREEN);
    u64 filetype = IdentifyFileType(path);
    u16* bitmap = NULL;
    u8* input = (u8*)malloc(max_size);
    u32 w = 0;
    u32 h = 0;
    u32 ret = 1;

    if (!input)
        return ret;

    u32 input_size = FileGetData(path, input, max_size, 0);
    if (input_size && (input_size < max_size)) {
        if (filetype & GFX_PNG) {
            bitmap = PNG_Decompress(input, input_size, &w, &h);
            if (bitmap != NULL) ret = 0;
        }
    }

    if ((ret == 0) && w && h && (w <= SCREEN_WIDTH(ALT_SCREEN)) && (h <= SCREEN_HEIGHT)) {
        ClearScreenF(true, true, COLOR_STD_BG);
        DrawBitmap(ALT_SCREEN, -1, -1, w, h, bitmap);
        ShowString("%s", STR_PRESS_A_TO_CONTINUE);
        while(!(InputWait(0) & (BUTTON_A | BUTTON_B)));
        ClearScreenF(true, true, COLOR_STD_BG);
    } else ret = 1;

    free(bitmap);
    free(input);
    return ret;
}

u32 FileHexViewer(const char* path) {
    const u32 max_data = (SCREEN_HEIGHT / FONT_HEIGHT_EXT) * 16 * ((FONT_WIDTH_EXT > 4) ? 1 : 2);
    static u32 mode = 0;
    u8* data = NULL;
    u8* bottom_cpy = (u8*) malloc(SCREEN_SIZE_BOT); // a copy of the bottom screen framebuffer
    u32 fsize = FileGetSize(path);

    bool dual_screen = 0;
    int x_off = 0, x_hex = 0, x_ascii = 0;
    u32 vpad = 0, hlpad = 0, hrpad = 0;
    u32 rows = 0, cols = 0;
    u32 total_shown = 0;
    u32 total_data = 0;

    u32 last_mode = 0xFF;
    u32 last_offset = (u32) -1;
    u32 offset = 0;

    u8  found_data[64 + 1] = { 0 };
    u32 found_offset = (u32) -1;
    u32 found_size = 0;

    static const u32 edit_bsize = 0x4000; // should be multiple of 0x200 * 2
    bool edit_mode = false;
    u8* buffer = (u8*) malloc(edit_bsize);
    u8* buffer_cpy = (u8*) malloc(edit_bsize);
    u32 edit_start = 0;
    int cursor = 0;

    if (!bottom_cpy || !buffer || !buffer_cpy) {
        if (bottom_cpy) free(bottom_cpy);
        if (buffer) free(buffer);
        if (buffer_cpy) free(buffer_cpy);
        return 1;
    }

    static bool show_instr = true;
    const char* instr = STR_HEXEDITOR_CONTROLS;
    if (show_instr) { // show one time instructions
        ShowPrompt(false, "%s", instr);
        show_instr = false;
    }

    if (MAIN_SCREEN != TOP_SCREEN) ShowString("%s", instr);
    memcpy(bottom_cpy, BOT_SCREEN, SCREEN_SIZE_BOT);

    data = buffer;
    while (true) {
        if (mode != last_mode) {
            if (FONT_WIDTH_EXT <= 5) {
                mode = 0;
                vpad = 1;
                hlpad = hrpad = 2;
                cols = 16;
                x_off = (SCREEN_WIDTH_TOP - SCREEN_WIDTH_BOT) / 2;
                x_ascii = SCREEN_WIDTH_TOP - x_off - (FONT_WIDTH_EXT * cols);
                x_hex = ((SCREEN_WIDTH_TOP - ((hlpad + (2*FONT_WIDTH_EXT) + hrpad) * cols)) / 2) -
                    (((cols - 8) / 2) * FONT_WIDTH_EXT);
                dual_screen = true;
            } else if (FONT_WIDTH_EXT <= 6) {
                if (mode == 1) {
                    vpad = 0;
                    hlpad = hrpad = 1;
                    cols = 16;
                    x_off = 0;
                    x_ascii = SCREEN_WIDTH_TOP - (FONT_WIDTH_EXT * cols);
                    x_hex = x_off + (8*FONT_WIDTH_EXT) + 16;
                    dual_screen = false;
                } else {
                    mode = 0;
                    vpad = 0;
                    hlpad = hrpad = 3;
                    cols = 8;
                    x_off = 30 + (SCREEN_WIDTH_TOP - SCREEN_WIDTH_BOT) / 2;
                    x_ascii = SCREEN_WIDTH_TOP - x_off - (FONT_WIDTH_EXT * cols);
                    x_hex = (SCREEN_WIDTH_TOP - ((hlpad + (2*FONT_WIDTH_EXT) + hrpad) * cols)) / 2;
                    dual_screen = true;
                }
            } else switch (mode) { // display mode
                case 1:
                    vpad = hlpad = hrpad = 1;
                    cols = 12;
                    x_off = 0;
                    x_ascii = SCREEN_WIDTH_TOP - (FONT_WIDTH_EXT * cols);
                    x_hex = ((SCREEN_WIDTH_TOP - ((hlpad + (2*FONT_WIDTH_EXT) + hrpad) * cols) -
                        ((cols - 8) * FONT_WIDTH_EXT)) / 2);
                    dual_screen = false;
                    break;
                case 2:
                    vpad = 1;
                    hlpad = 0;
                    hrpad = 1 + 8 - FONT_WIDTH_EXT;
                    cols = 16;
                    x_off = -1;
                    x_ascii = SCREEN_WIDTH_TOP - (FONT_WIDTH_EXT * cols);
                    x_hex = 0;
                    dual_screen = false;
                    break;
                case 3:
                    vpad = hlpad = hrpad = 1;
                    cols = 16;
                    x_off = ((SCREEN_WIDTH_TOP - ((hlpad + (2*FONT_WIDTH_EXT) + hrpad) * cols)
                        - 12 - (8*FONT_WIDTH_EXT)) / 2);
                    x_ascii = -1;
                    x_hex = x_off + (8*FONT_WIDTH_EXT) + 12;
                    dual_screen = false;
                    break;
                default:
                    mode = 0;
                    vpad = hlpad = hrpad = 2;
                    cols = 8;
                    x_off = (SCREEN_WIDTH_TOP - SCREEN_WIDTH_BOT) / 2;
                    x_ascii = SCREEN_WIDTH_TOP - x_off - (FONT_WIDTH_EXT * cols);
                    x_hex = (SCREEN_WIDTH_TOP - ((hlpad + (2*FONT_WIDTH_EXT) + hrpad) * cols)) / 2;
                    dual_screen = true;
                    break;
            }
            rows = (dual_screen ? 2 : 1) * SCREEN_HEIGHT / (FONT_HEIGHT_EXT + (2*vpad));
            total_shown = rows * cols;
            last_mode = mode;
            ClearScreen(TOP_SCREEN, COLOR_STD_BG);
            if (dual_screen) ClearScreen(BOT_SCREEN, COLOR_STD_BG);
            else memcpy(BOT_SCREEN, bottom_cpy, SCREEN_SIZE_BOT);
        }
        // fix offset (if required)
        if (offset % cols) offset -= (offset % cols); // fix offset (align to cols)
        if (offset + total_shown - cols > fsize) // if offset too big
            offset = (total_shown > fsize) ? 0 : (fsize + cols - total_shown - (fsize % cols));
        // get data, using max data size (if new offset)
        if (offset != last_offset) {
            if (!edit_mode) {
                total_data = FileGetData(path, data, max_data, offset);
            } else { // edit mode - read from memory
                if ((offset < edit_start) || (offset + max_data > edit_start + edit_bsize))
                    offset = last_offset; // we don't expect this to happen
                total_data = (fsize - offset >= max_data) ? max_data : fsize - offset;
                data = buffer + (offset - edit_start);
            }
            last_offset = offset;
        }

        // display data on screen
        for (u32 row = 0; row < rows; row++) {
            char ascii[16 + 1] = { 0 };
            u32 y = row * (FONT_HEIGHT_EXT + (2*vpad)) + vpad;
            u32 curr_pos = row * cols;
            u32 cutoff = (curr_pos >= total_data) ? 0 : (total_data >= curr_pos + cols) ? cols : total_data - curr_pos;
            u16* screen = TOP_SCREEN;
            u32 x0 = 0;

            // marked offsets handling
            s32 marked0 = 0, marked1 = 0;
            if ((found_size > 0) &&
                (found_offset + found_size > offset + curr_pos) &&
                (found_offset < offset + curr_pos + cols)) {
                marked0 = (s32) found_offset - (offset + curr_pos);
                marked1 = marked0 + found_size;
                if (marked0 < 0) marked0 = 0;
                if (marked1 > (s32) cols) marked1 = (s32) cols;
            }

            // switch to bottom screen
            if (y >= SCREEN_HEIGHT) {
                y -= SCREEN_HEIGHT;
                screen = BOT_SCREEN;
                x0 = 40;
            }

            memcpy(ascii, data + curr_pos, cols);
            for (u32 col = 0; col < cols; col++)
                if ((col >= cutoff) || (ascii[col] == 0x00)) ascii[col] = ' ';

            // draw offset / ASCII representation
            if (x_off >= 0) DrawStringF(screen, x_off - x0, y, cutoff ? COLOR_HVOFFS : COLOR_HVOFFSI,
                COLOR_STD_BG, "%08X", (unsigned int) (offset + curr_pos));
            if (x_ascii >= 0) {
                for (u32 i = 0; i < cols; i++)
                    DrawCharacter(screen, ascii[i], x_ascii - x0 + (FONT_WIDTH_EXT * i), y, COLOR_HVASCII, COLOR_STD_BG);
                for (u32 i = (u32) marked0; i < (u32) marked1; i++)
                    DrawCharacter(screen, ascii[i % cols], x_ascii - x0 + (FONT_WIDTH_EXT * i), y, COLOR_MARKED, COLOR_STD_BG);
                if (edit_mode && ((u32) cursor / cols == row)) DrawCharacter(screen, ascii[cursor % cols],
                    x_ascii - x0 + FONT_WIDTH_EXT * (cursor % cols), y, COLOR_RED, COLOR_STD_BG);
            }

            // draw HEX values
            for (u32 col = 0; (col < cols) && (x_hex >= 0); col++) {
                u32 x = (x_hex + hlpad) + (((2*FONT_WIDTH_EXT) + hrpad + hlpad) * col) - x0;
                u32 hex_color = (edit_mode && ((u32) cursor == curr_pos + col)) ? COLOR_RED :
                    (((s32) col >= marked0) && ((s32) col < marked1)) ? COLOR_MARKED : COLOR_HVHEX(col);
                if (col < cutoff)
                    DrawStringF(screen, x, y, hex_color, COLOR_STD_BG, "%02X", (unsigned int) data[curr_pos + col]);
                else DrawStringF(screen, x, y, hex_color, COLOR_STD_BG, "  ");
            }
        }

        // handle user input
        u32 pad_state = InputWait(0);
        if (!edit_mode) { // standard viewer mode
            u32 step_ud = (pad_state & BUTTON_R1) ? (0x1000  - (0x1000  % cols)) : cols;
            u32 step_lr = (pad_state & BUTTON_R1) ? (0x10000 - (0x10000 % cols)) : total_shown;
            if (pad_state & BUTTON_DOWN) offset += step_ud;
            else if (pad_state & BUTTON_RIGHT) offset += step_lr;
            else if (pad_state & BUTTON_UP) offset = (offset > step_ud) ? offset - step_ud : 0;
            else if (pad_state & BUTTON_LEFT) offset = (offset > step_lr) ? offset - step_lr : 0;
            else if ((pad_state & BUTTON_R1) && (pad_state & BUTTON_Y)) mode++;
            else if ((pad_state & BUTTON_A) && total_data) edit_mode = true;
            else if (pad_state & (BUTTON_B|BUTTON_START)) break;
            else if (found_size && (pad_state & BUTTON_R1) && (pad_state & BUTTON_X)) {
                found_offset = FileFindData(path, found_data, found_size, found_offset + 1);
                if (found_offset == (u32) -1) {
                    ShowPrompt(false, "%s", STR_NOT_FOUND);
                    found_size = 0;
                } else offset = found_offset;
                if (MAIN_SCREEN == TOP_SCREEN) ClearScreen(TOP_SCREEN, COLOR_STD_BG);
                else if (dual_screen) ClearScreen(BOT_SCREEN, COLOR_STD_BG);
                else memcpy(BOT_SCREEN, bottom_cpy, SCREEN_SIZE_BOT);
            } else if (pad_state & BUTTON_X) {
                const char* optionstr[3] = { STR_GO_TO_OFFSET, STR_SEARCH_FOR_STRING, STR_SEARCH_FOR_DATA };
                u32 user_select = ShowSelectPrompt(3, optionstr, STR_CURRENT_OFFSET_SELECT_ACTION, offset);
                if (user_select == 1) { // -> goto offset
                    u64 new_offset = ShowHexPrompt(offset, 8, STR_CURRENT_OFFSET_ENTER_NEW, offset);
                    if (new_offset != (u64) -1) offset = new_offset;
                } else if (user_select == 2) {
                    if (!found_size) *found_data = 0;
                    if (ShowKeyboardOrPrompt((char*) found_data, 64 + 1, "%s", STR_ENTER_SEARCH_REPEAT_SEARCH)) {
                        found_size = strnlen((char*) found_data, 64);
                        found_offset = FileFindData(path, found_data, found_size, offset);
                        if (found_offset == (u32) -1) {
                            ShowPrompt(false, "%s", STR_NOT_FOUND);
                            found_size = 0;
                        } else offset = found_offset;
                    }
                } else if (user_select == 3) {
                    u32 size = found_size;
                    if (ShowDataPrompt(found_data, &size, "%s", STR_ENTER_SEARCH_REPEAT_SEARCH)) {
                        found_size = size;
                        found_offset = FileFindData(path, found_data, size, offset);
                        if (found_offset == (u32) -1) {
                            ShowPrompt(false, "%s", STR_NOT_FOUND);
                            found_size = 0;
                        } else offset = found_offset;
                    }
                }
                if (MAIN_SCREEN == TOP_SCREEN) ClearScreen(TOP_SCREEN, COLOR_STD_BG);
                else if (dual_screen) ClearScreen(BOT_SCREEN, COLOR_STD_BG);
                else memcpy(BOT_SCREEN, bottom_cpy, SCREEN_SIZE_BOT);
            }
            if (edit_mode && CheckWritePermissions(path)) { // setup edit mode
                found_size = 0;
                found_offset = (u32) -1;
                cursor = 0;
                edit_start = ((offset - (offset % 0x200) <= (edit_bsize / 2)) || (fsize < edit_bsize)) ? 0 :
                    offset - (offset % 0x200) - (edit_bsize / 2);
                FileGetData(path, buffer, edit_bsize, edit_start);
                memcpy(buffer_cpy, buffer, edit_bsize);
                data = buffer + (offset - edit_start);
            } else edit_mode = false;
        } else { // editor mode
            if (pad_state & (BUTTON_B|BUTTON_START)) {
                edit_mode = false;
                // check for user edits
                u32 diffs = 0;
                for (u32 i = 0; i < edit_bsize; i++) if (buffer[i] != buffer_cpy[i]) diffs++;
                if (diffs && ShowPrompt(true, STR_MADE_EDITS_SAVE_CHANGES, diffs))
                    if (!FileSetData(path, buffer, min(edit_bsize, (fsize - edit_start)), edit_start, false))
                        ShowPrompt(false, "%s", STR_FAILED_WRITING_TO_FILE);
                data = buffer;
                last_offset = (u32) -1; // force reload from file
            } else if (pad_state & BUTTON_A) {
                if (pad_state & BUTTON_DOWN) data[cursor]--;
                else if (pad_state & BUTTON_UP) data[cursor]++;
                else if (pad_state & BUTTON_RIGHT) data[cursor] += 0x10;
                else if (pad_state & BUTTON_LEFT) data[cursor] -= 0x10;
            } else {
                if (pad_state & BUTTON_DOWN) cursor += cols;
                else if (pad_state & BUTTON_UP) cursor -= cols;
                else if (pad_state & BUTTON_RIGHT) cursor++;
                else if (pad_state & BUTTON_LEFT) cursor--;
                // fix cursor position
                if (cursor < 0) {
                    if (offset >= cols) {
                        offset -= cols;
                        cursor += cols;
                    } else cursor = 0;
                } else if (((u32) cursor >= total_data) && (total_data < total_shown)) {
                    cursor = total_data - 1;
                } else if ((u32) cursor >= total_shown) {
                    if (offset + total_shown == fsize) {
                        cursor = total_shown - 1;
                    } else {
                        offset += cols;
                        cursor = (offset + cursor >= fsize) ? fsize - offset - 1 : cursor - cols;
                    }
                }
            }
        }
    }

    ClearScreen(TOP_SCREEN, COLOR_STD_BG);
    if (MAIN_SCREEN == TOP_SCREEN) memcpy(BOT_SCREEN, bottom_cpy, SCREEN_SIZE_BOT);
    else ClearScreen(BOT_SCREEN, COLOR_STD_BG);

    free(bottom_cpy);
    free(buffer);
    free(buffer_cpy);
    return 0;
}

u32 ShaCalculator(const char* path, bool sha1) {
    const u8 hashlen = sha1 ? 20 : 32;
    u32 drvtype = DriveType(path);
    char pathstr[UTF_BUFFER_BYTESIZE(32)];
    u8 hash[32];
    TruncateString(pathstr, path, 32, 8);
    if (!FileGetSha(path, hash, 0, 0, sha1)) {
        ShowPrompt(false, STR_CALCULATING_SHA_FAILED, sha1 ? "1" : "256");
        return 1;
    } else {
        static char pathstr_prev[UTF_BUFFER_BYTESIZE(32)] = { 0 };
        static u8 hash_prev[32] = { 0 };
        char sha_path[256];
        u8 sha_file[32];

        snprintf(sha_path, sizeof(sha_path), "%s.sha%c", path, sha1 ? '1' : '\0');
        bool have_sha = (FileGetData(sha_path, sha_file, hashlen, 0) == hashlen);
        bool match_sha = have_sha && (memcmp(hash, sha_file, hashlen) == 0);
        bool match_prev = (memcmp(hash, hash_prev, hashlen) == 0);
        bool write_sha = (!have_sha || !match_sha) && (drvtype & DRV_SDCARD); // writing only on SD
        char hash_str[32+1+32+1];
        if (sha1)
            snprintf(hash_str, sizeof(hash_str), "%016llX%04X\n%016llX%04X", getbe64(hash + 0), getbe16(hash + 8),
            getbe64(hash + 10), getbe16(hash + 18));
        else
            snprintf(hash_str, sizeof(hash_str), "%016llX%016llX\n%016llX%016llX", getbe64(hash + 0), getbe64(hash + 8),
            getbe64(hash + 16), getbe64(hash + 24));
        if (ShowPrompt(write_sha, "%s\n%s%s%s%s%s",
            pathstr, hash_str,
            (have_sha) ? ((match_sha) ? STR_SHA_VERIFICATION_PASSED : STR_SHA_VERIFICATION_FAILED) : "",
            (match_prev) ? STR_IDENTICAL_WITH_PREVIOUS : "",
            (match_prev) ? pathstr_prev : "",
            (sha1) ? STR_WRITE_SHA1_FILE : STR_WRITE_SHA_FILE) && write_sha) {
            FileSetData(sha_path, hash, hashlen, 0, true);
        }

        strncpy(pathstr_prev, pathstr, UTF_BUFFER_BYTESIZE(32));
        memcpy(hash_prev, hash, hashlen);
    }

    return 0;
}

u32 CmacCalculator(const char* path) {
    char pathstr[UTF_BUFFER_BYTESIZE(32)];
    TruncateString(pathstr, path, 32, 8);
    if (IdentifyFileType(path) != GAME_CMD) {
        u8 cmac[16] __attribute__((aligned(4)));
        if (CalculateFileCmac(path, cmac) != 0) {
            ShowPrompt(false, "%s", STR_CALCULATING_CMAC_FAILED);
            return 1;
        } else {
            u8 cmac_file[16];
            bool identical = ((ReadFileCmac(path, cmac_file) == 0) && (memcmp(cmac, cmac_file, 16) == 0));
            if (ShowPrompt(!identical, "%s\n%016llX%016llX\n%s%s",
                pathstr, getbe64(cmac + 0), getbe64(cmac + 8),
                (identical) ? STR_CMAC_VERIFICATION_PASSED : STR_CMAC_VERIFICATION_FAILED,
                (!identical) ? STR_FIX_CMAC_IN_FILE : "") &&
                !identical && (WriteFileCmac(path, cmac, true) != 0)) {
                ShowPrompt(false, "%s", STR_FIXING_CMAC_FAILED);
            }
        }
    } else { // special handling for CMD files
        bool correct = (CheckCmdCmac(path) == 0);
        if (ShowPrompt(!correct, "%s\nXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n%s%s",
            pathstr, (correct) ? STR_CMAC_VERIFICATION_PASSED : STR_CMAC_VERIFICATION_FAILED,
            (!correct) ? STR_FIX_CMAC_IN_FILE : "") &&
            !correct && (FixCmdCmac(path, true) != 0)) {
            ShowPrompt(false, "%s", STR_FIXING_CMAC_FAILED);
        }
    }


    return 0;
}

u32 StandardCopy(u32* cursor, u32* scroll) {
    DirEntry* curr_entry = &(current_dir->entry[*cursor]);
    u32 n_marked = 0;
    if (curr_entry->marked) {
        for (u32 i = 0; i < current_dir->n_entries; i++)
            if (current_dir->entry[i].marked) n_marked++;
    }

    u32 flags = BUILD_PATH;
    if ((n_marked > 1) && ShowPrompt(true, STR_COPY_ALL_SELECTED_ITEMS, n_marked)) {
        u32 n_success = 0;
        for (u32 i = 0; i < current_dir->n_entries; i++) {
            const char* path = current_dir->entry[i].path;
            if (!current_dir->entry[i].marked)
                continue;
            flags |= ASK_ALL;
            DrawDirContents(current_dir, (*cursor = i), scroll);
            if (PathCopy(OUTPUT_PATH, path, &flags)) n_success++;
            else { // on failure: show error, break
                char currstr[UTF_BUFFER_BYTESIZE(32)];
                TruncateString(currstr, path, 32, 12);
                ShowPrompt(false, "%s\n%s", currstr, STR_FAILED_COPYING_ITEM);
                break;
            }
            current_dir->entry[i].marked = false;
        }
        if (n_success) ShowPrompt(false, STR_ITEMS_COPIED_TO_OUT, n_success, OUTPUT_PATH);
    } else {
        char pathstr[UTF_BUFFER_BYTESIZE(32)];
        TruncateString(pathstr, curr_entry->path, 32, 8);
        if (!PathCopy(OUTPUT_PATH, curr_entry->path, &flags))
            ShowPrompt(false, "%s\n%s", pathstr, STR_FAILED_COPYING_ITEM);
        else ShowPrompt(false, STR_PATH_COPIED_TO_OUT, pathstr, OUTPUT_PATH);
    }

    return 0;
}

u32 CartRawDump(void) {
    CartData* cdata = (CartData*) malloc(sizeof(CartData));
    char dest[256];
    char cname[24];
    char bytestr[32];
    u64 dsize = 0;

    if (!cdata ||(InitCartRead(cdata) != 0) || (GetCartName(cname, cdata) != 0)) {
        ShowPrompt(false, "%s", STR_CART_INIT_FAILED);
        free(cdata);
        return 1;
    }

    // input dump size
    dsize = cdata->cart_size;
    FormatBytes(bytestr, dsize);
    dsize = ShowHexPrompt(dsize, 8, STR_CART_DETECTED_SIZE_INPUT_BELOW, cname, bytestr);
    if (!dsize || (dsize == (u64) -1)) {
        free(cdata);
        return 1;
    }

    // for NDS carts: ask for secure area encryption
    if (cdata->cart_type & CART_NTR)
        SetSecureAreaEncryption(
            !ShowPrompt(true, STR_NDS_CART_DECRYPT_SECURE_AREA, cname));

    // destination path
    snprintf(dest, sizeof(dest), "%s/%s_%08llX.%s",
        OUTPUT_PATH, cname, dsize, (cdata->cart_type & CART_CTR) ? "3ds" : "nds");

    // buffer allocation
    u8* buf = (u8*) malloc(STD_BUFFER_SIZE);
    if (!buf) { // this will not happen
        free(cdata);
        return 1;
    }

    // actual cart dump
    u32 ret = 0;
    PathDelete(dest);
    ShowProgress(0, 0, cname);
    for (u64 p = 0; p < dsize; p += STD_BUFFER_SIZE) {
        u64 len = min((dsize - p), STD_BUFFER_SIZE);
        if ((ReadCartBytes(buf, p, len, cdata, false) != 0) ||
            (fvx_qwrite(dest, buf, p, len, NULL) != FR_OK) ||
            !ShowProgress(p, dsize, cname)) {
            PathDelete(dest);
            ret = 1;
            break;
        }
    }

    if (ret) ShowPrompt(false, STR_FAILED_DUMPING_CART, cname);
    else ShowPrompt(false, STR_PATH_DUMPED_TO_OUT, cname, OUTPUT_PATH);
    
    free(buf);
    free(cdata);
    return ret;
}

u32 DirFileAttrMenu(const char* path, const char *name) {
    bool drv = (path[2] == '\0');
    bool vrt = (!drv); // will be checked below
    char namestr[UTF_BUFFER_BYTESIZE(128)], datestr[UTF_BUFFER_BYTESIZE(32)], attrstr[UTF_BUFFER_BYTESIZE(128)], sizestr[UTF_BUFFER_BYTESIZE(192)];
    FILINFO fno;
    u8 new_attrib;

    // create mutiline name string
    MultiLineString(namestr, name, 31, 4);

    // preparations: create file info, date string
    if (!drv) {
        if (fvx_stat(path, &fno) != FR_OK) return 1;
        vrt = (fno.fattrib & AM_VRT);
        new_attrib = fno.fattrib;
        snprintf(datestr, sizeof(datestr), "%s: %04d-%02d-%02d %02d:%02d:%02d\n",
            (fno.fattrib & AM_DIR) ? STR_CREATED : STR_MODIFIED,
            1980 + ((fno.fdate >> 9) & 0x7F), (fno.fdate >> 5) & 0xF, fno.fdate & 0x1F,
            (fno.ftime >> 11) & 0x1F, (fno.ftime >> 5) & 0x3F, (fno.ftime & 0x1F) << 1);
    } else {
        *datestr = '\0';
        *attrstr = '\0';
        new_attrib = 0;
    }

    // create size string
    if (drv || (fno.fattrib & AM_DIR)) { // for dirs and drives
        char bytestr[32];
        u64 tsize = 0;
        u32 tdirs = 0;
        u32 tfiles = 0;

        // this may take a while...
        ShowString("%s", drv ? STR_ANALYZING_DRIVE : STR_ANALYZING_DIR);
        if (!DirInfo(path, &tsize, &tdirs, &tfiles))
            return 1;
        FormatBytes(bytestr, tsize);

        if (drv) { // drive specific
            char freestr[32], drvsstr[32], usedstr[32];
            FormatBytes(freestr, GetFreeSpace(path));
            FormatBytes(drvsstr, GetTotalSpace(path));
            FormatBytes(usedstr, GetTotalSpace(path) - GetFreeSpace(path));
            snprintf(sizestr, sizeof(sizestr), STR_N_FILES_N_SUBDIRS_TOTAL_SIZE_FREE_USED_TOTAL,
                tfiles, tdirs, bytestr, freestr, usedstr, drvsstr);
        } else { // dir specific
            snprintf(sizestr, sizeof(sizestr), STR_N_FILES_N_SUBDIRS_TOTAL_SIZE, tfiles, tdirs, bytestr);
        }
    } else { // for files
        char bytestr[32];
        FormatBytes(bytestr, fno.fsize);
        snprintf(sizestr, sizeof(sizestr), STR_FILESIZE_X, bytestr);
    }

    while(true) {
        if (!drv) {
            snprintf(attrstr, sizeof(attrstr),
                STR_READONLY_HIDDEN_SYSTEM_ARCHIVE_VIRTUAL,
                (new_attrib & AM_RDO) ? 'X' : ' ', vrt ? "" : "↑",
                (new_attrib & AM_HID) ? 'X' : ' ', vrt ? "" : "↓",
                (new_attrib & AM_SYS) ? 'X' : ' ', vrt ? "" : "→",
                (new_attrib & AM_ARC) ? 'X' : ' ', vrt ? "" : "←",
                vrt ? 'X' : ' ', vrt ? "" : "  ",
                vrt ? "" : STR_UDRL_CHANGE_ATTRIBUTES
            );
        }

        ShowString(
            "%s\n \n"   // name
            "%s"        // date (not for drives)
            "%s\n"      // size
            "%s \n"     // attr (not for drives)
            "%s\n",     // options
            namestr, datestr, sizestr, attrstr,
            (drv || vrt || (new_attrib == fno.fattrib)) ? STR_A_TO_CONTINUE : STR_A_APPLY_B_CANCEL
        );

        while(true) {
            u32 pad_state = InputWait(0);

            if (pad_state & (BUTTON_A | BUTTON_B)) {
                if (!drv && !vrt) {
                    const u8 mask = (AM_RDO | AM_HID | AM_SYS | AM_ARC);
                    bool apply = (new_attrib != fno.fattrib) && (pad_state & BUTTON_A);
                    if (apply && !PathAttr(path, new_attrib & mask, mask)) {
                        ShowPrompt(false, "%s\n%s", namestr, STR_FAILED_TO_SET_ATTRIBUTES);
                    }
                }
                ClearScreenF(true, false, COLOR_STD_BG);
                return 0;
            }

            if (!drv && !vrt && (pad_state & BUTTON_ARROW)) {
                switch (pad_state & BUTTON_ARROW) {
                    case BUTTON_UP:
                        new_attrib ^= AM_RDO;
                        break;
                    case BUTTON_DOWN:
                        new_attrib ^= AM_HID;
                        break;
                    case BUTTON_RIGHT:
                        new_attrib ^= AM_SYS;
                        break;
                    case BUTTON_LEFT:
                        new_attrib ^= AM_ARC;
                        break;
                }
                break;
            }
        }
    }
}

u32 FileHandlerMenu(char* current_path, u32* cursor, u32* scroll, PaneData** pane) {
    const char* file_path = (&(current_dir->entry[*cursor]))->path;
    const char* file_name = (&(current_dir->entry[*cursor]))->name;
    const char* optionstr[16];

    // check for file lock
    if (!FileUnlock(file_path)) return 1;

    u64 filetype = IdentifyFileType(file_path);
    u32 drvtype = DriveType(file_path);
    u64 tid = GetGameFileTitleId(file_path);

    bool in_output_path = (strncasecmp(current_path, OUTPUT_PATH, 256) == 0);

    // don't handle TMDs inside the game drive, won't work properly anyways
    if ((filetype & GAME_TMD) && (drvtype & DRV_GAME)) filetype &= ~GAME_TMD;

    // special stuff, only available for known filetypes (see int special below)
    bool mountable = (FTYPE_MOUNTABLE(filetype) && !(drvtype & DRV_IMAGE) &&
        !((drvtype & (DRV_SYSNAND|DRV_EMUNAND)) && (drvtype & DRV_VIRTUAL) && (filetype & IMG_FAT)));
    bool verificable = (FTYPE_VERIFICABLE(filetype));
    bool decryptable = (FTYPE_DECRYPTABLE(filetype));
    bool encryptable = (FTYPE_ENCRYPTABLE(filetype));
    bool cryptable_inplace = ((encryptable||decryptable) && !in_output_path && (*current_path == '0'));
    bool cia_buildable = (FTYPE_CIABUILD(filetype));
    bool cia_buildable_legit = (FTYPE_CIABUILD_L(filetype));
    bool cia_installable = (FTYPE_CIAINSTALL(filetype)) && !(drvtype & DRV_CTRNAND) &&
        !(drvtype & DRV_TWLNAND) && !(drvtype & DRV_ALIAS) && !(drvtype & DRV_IMAGE);
    bool tik_installable = (FTYPE_TIKINSTALL(filetype)) && !(drvtype & DRV_IMAGE);
    bool tik_dumpable = (FTYPE_TIKDUMP(filetype));
    bool cif_installable = (FTYPE_CIFINSTALL(filetype)) && !(drvtype & DRV_IMAGE);
    bool uninstallable = (FTYPE_UNINSTALL(filetype));
    bool cxi_dumpable = (FTYPE_CXIDUMP(filetype));
    bool tik_buildable = (FTYPE_TIKBUILD(filetype)) && !in_output_path;
    bool key_buildable = (FTYPE_KEYBUILD(filetype)) && !in_output_path &&
        !((drvtype & DRV_VIRTUAL) && (drvtype & DRV_SYSNAND));
    bool titleinfo = (FTYPE_TITLEINFO(filetype));
    bool renamable = (FTYPE_RENAMABLE(filetype)) && !(drvtype & DRV_VIRTUAL) && !(drvtype & DRV_ALIAS) &&
        !(drvtype & DRV_CTRNAND) && !(drvtype & DRV_TWLNAND) && !(drvtype & DRV_IMAGE);
    bool trimable = (FTYPE_TRIMABLE(filetype)) && !(drvtype & DRV_VIRTUAL) && !(drvtype & DRV_ALIAS) &&
        !(drvtype & DRV_CTRNAND) && !(drvtype & DRV_TWLNAND) && !(drvtype & DRV_IMAGE);
    bool transferable = (FTYPE_TRANSFERABLE(filetype) && IS_UNLOCKED && (drvtype & DRV_FAT));
    bool hsinjectable = (FTYPE_HASCODE(filetype));
    bool extrcodeable = (FTYPE_HASCODE(filetype));
    bool restorable = (FTYPE_RESTORABLE(filetype) && IS_UNLOCKED && !(drvtype & DRV_SYSNAND));
    bool ebackupable = (FTYPE_EBACKUP(filetype));
    bool ncsdfixable = (FTYPE_NCSDFIXABLE(filetype));
    bool xorpadable = (FTYPE_XORPAD(filetype));
    bool keyinitable = (FTYPE_KEYINIT(filetype)) && !((drvtype & DRV_VIRTUAL) && (drvtype & DRV_SYSNAND));
    bool keyinstallable = (FTYPE_KEYINSTALL(filetype)) && !((drvtype & DRV_VIRTUAL) && (drvtype & DRV_SYSNAND));
    bool scriptable = (FTYPE_SCRIPT(filetype));
    bool fontable = (FTYPE_FONT(filetype));
    bool translationable = (FTYPE_TRANSLATION(filetype));
    bool viewable = (FTYPE_GFX(filetype));
    bool setable = (FTYPE_SETABLE(filetype));
    bool bootable = (FTYPE_BOOTABLE(filetype));
    bool installable = (FTYPE_INSTALLABLE(filetype));
    bool agbexportable = (FTYPE_AGBSAVE(filetype) && (drvtype & DRV_VIRTUAL) && (drvtype & DRV_SYSNAND));
    bool agbimportable = (FTYPE_AGBSAVE(filetype) && (drvtype & DRV_VIRTUAL) && (drvtype & DRV_SYSNAND));
    bool luascriptable = (FTYPE_LUA(filetype));

    char cxi_path[256] = { 0 }; // special options for TMD
    if ((filetype & GAME_TMD) &&
        (GetTmdContentPath(cxi_path, file_path) == 0) &&
        (PathExist(cxi_path))) {
        u64 filetype_cxi = IdentifyFileType(cxi_path);
        mountable = (FTYPE_MOUNTABLE(filetype_cxi) && !(drvtype & DRV_IMAGE));
        extrcodeable = (FTYPE_HASCODE(filetype_cxi));
    }

    bool special_opt =
        mountable || verificable || decryptable || encryptable || cia_buildable || cia_buildable_legit ||
        cxi_dumpable || tik_buildable || key_buildable || titleinfo || renamable || trimable || transferable ||
        hsinjectable || restorable || xorpadable || ebackupable || ncsdfixable || extrcodeable || keyinitable ||
        keyinstallable || bootable || scriptable || fontable || translationable || viewable || installable ||
        agbexportable || agbimportable || cia_installable || tik_installable || tik_dumpable || cif_installable ||
	luascriptable;

    char pathstr[UTF_BUFFER_BYTESIZE(32)];
    TruncateString(pathstr, file_path, 32, 8);

    char tidstr[32] = { 0 };
    if (tid) snprintf(tidstr, sizeof(tidstr), "\ntid: <%016llX>", tid);

    u32 n_marked = 0;
    if ((&(current_dir->entry[*cursor]))->marked) {
        for (u32 i = 0; i < current_dir->n_entries; i++)
            if (current_dir->entry[i].marked) n_marked++;
    }

    // main menu processing
    int n_opt = 0;
    int special = (special_opt) ? ++n_opt : -1;
    int hexviewer = ++n_opt;
    int textviewer = (filetype & TXT_GENERIC) ? ++n_opt : -1;
    int calcsha256 = ++n_opt;
    int calcsha1 = ++n_opt;
    int calccmac = (CheckCmacPath(file_path) == 0) ? ++n_opt : -1;
    int fileinfo = ++n_opt;
    int copystd = (!in_output_path) ? ++n_opt : -1;
    int inject = ((clipboard->n_entries == 1) &&
        (clipboard->entry[0].type == T_FILE) &&
        (strncmp(clipboard->entry[0].path, file_path, 256) != 0)) ?
        (int) ++n_opt : -1;
    int searchdrv = (DriveType(current_path) & DRV_SEARCH) ? ++n_opt : -1;
    int titleman = -1;
    if (DriveType(current_path) & DRV_TITLEMAN) {
        // special case: title manager (disable almost everything)
        hexviewer = textviewer = calcsha256 = calcsha1 = calccmac = fileinfo = copystd = inject = searchdrv = -1;
        special = 1;
        titleman = 2;
        n_opt = 2;
    }

    // format strings that need it
    char buildkeydb_str[256], buildtikdbenc_str[256], buildtikdbdec_str[256],
         copyto_str[256], decryptto_str[256], encryptto_str[256], extractexefs_str[256],
         initkeydb_str[256], installkeydb_str[256];
    snprintf(buildkeydb_str, sizeof(buildkeydb_str), STR_BUILD_X, KEYDB_NAME);
    snprintf(buildtikdbenc_str, sizeof(buildtikdbenc_str), STR_BUILD_X, TIKDB_NAME_ENC);
    snprintf(buildtikdbdec_str, sizeof(buildtikdbdec_str), STR_BUILD_X, TIKDB_NAME_DEC);
    snprintf(copyto_str, sizeof(copyto_str), STR_COPY_TO_OUT, OUTPUT_PATH);
    snprintf(decryptto_str, sizeof(decryptto_str), STR_DECRYPT_FILE_OUT, OUTPUT_PATH);
    snprintf(encryptto_str, sizeof(encryptto_str), STR_ENCRYPT_FILE_OUT, OUTPUT_PATH);
    snprintf(extractexefs_str, sizeof(extractexefs_str), STR_EXTRACT_X, EXEFS_CODE_NAME);
    snprintf(initkeydb_str, sizeof(initkeydb_str), STR_INIT_X, KEYDB_NAME);
    snprintf(installkeydb_str, sizeof(installkeydb_str), STR_INSTALL_X, KEYDB_NAME);

    if (special > 0) optionstr[special-1] =
        (filetype & IMG_NAND)   ? STR_NAND_IMAGE_OPTIONS   :
        (filetype & IMG_FAT)    ? (transferable) ? STR_CTRNAND_OPTIONS : STR_MOUNT_FAT_IMAGE :
        (filetype & GAME_CIA)   ? STR_CIA_IMAGE_OPTIONS    :
        (filetype & GAME_NCSD)  ? STR_NCSD_IMAGE_OPTIONS   :
        (filetype & GAME_NCCH)  ? STR_NCCH_IMAGE_OPTIONS   :
        (filetype & GAME_EXEFS) ? STR_MOUNT_AS_EXEFS_IMAGE :
        (filetype & GAME_ROMFS) ? STR_MOUNT_AS_ROMFS_IMAGE :
        (filetype & GAME_TMD)   ? STR_TMD_FILE_OPTIONS     :
        (filetype & GAME_CDNTMD)? STR_TMD_CDN_OPTIONS      :
        (filetype & GAME_TWLTMD)? STR_TMD_TWL_OPTIONS      :
        (filetype & GAME_TIE)   ? STR_MANAGE_TITLE         :
        (filetype & GAME_BOSS)  ? STR_BOSS_FILE_OPTIONS    :
        (filetype & GAME_NUSCDN)? STR_DECRYPT_NUS_CDN_FILE :
        (filetype & GAME_SMDH)  ? STR_SHOW_SMDH_TITLE_INFO :
        (filetype & GAME_NDS)   ? STR_NDS_IMAGE_OPTIONS    :
        (filetype & GAME_GBA)   ? STR_GBA_IMAGE_OPTIONS    :
        (filetype & GAME_TICKET)? STR_TICKET_OPTIONS       :
        (filetype & GAME_TAD)   ? STR_TAD_IMAGE_OPTIONS    :
        (filetype & GAME_3DSX)  ? STR_SHOW_3DSX_TITLE_INFO :
        (filetype & SYS_FIRM)   ? STR_FIRM_IMAGE_OPTIONS   :
        (filetype & SYS_AGBSAVE)? (agbimportable) ? STR_AGBSAVE_OPTIONS : STR_DUMP_GBA_VC_SAVE :
        (filetype & SYS_TICKDB) ? STR_TICKET_DB_OPTIONS    :
        (filetype & SYS_DIFF)   ? STR_MOUNT_AS_DIFF_IMAGE  :
        (filetype & SYS_DISA)   ? STR_MOUNT_AS_DISA_IAMGE  :
        (filetype & BIN_CIFNSH) ? STR_INSTALL_CIFINISH_BIN :
        (filetype & BIN_TIKDB)  ? STR_TITLEKEY_OPTIONS     :
        (filetype & BIN_KEYDB)  ? STR_AESKEYDB_OPTIONS     :
        (filetype & BIN_LEGKEY) ? buildkeydb_str           :
        (filetype & BIN_NCCHNFO)? STR_NCCHINFO_OPTIONS     :
        (filetype & TXT_SCRIPT) ? STR_EXECUTE_GM9_SCRIPT   :
        (filetype & TXT_LUA)    ? STR_EXECUTE_LUA_SCRIPT   :
        (FTYPE_FONT(filetype))  ? STR_FONT_OPTIONS         :
        (filetype & TRANSLATION)? STR_LANGUAGE_OPTIONS     :
        (filetype & GFX_PNG)    ? STR_VIEW_PNG_FILE        :
        (filetype & HDR_NAND)   ? STR_REBUILD_NCSD_HEADER  :
        (filetype & NOIMG_NAND) ? STR_REBUILD_NCSD_HEADER  : "???";
    optionstr[hexviewer-1] = STR_SHOW_IN_HEXEDITOR;
    optionstr[calcsha256-1] = STR_CALCULATE_SHA256;
    optionstr[calcsha1-1] = STR_CALCULATE_SHA1;
    optionstr[fileinfo-1] = STR_SHOW_FILE_INFO;
    if (textviewer > 0) optionstr[textviewer-1] = STR_SHOW_IN_TEXTVIEWER;
    if (calccmac > 0) optionstr[calccmac-1] = STR_CALCULATE_CMAC;
    if (copystd > 0) optionstr[copystd-1] = copyto_str;
    if (inject > 0) optionstr[inject-1] = STR_INJECT_DATA_AT_OFFSET;
    if (searchdrv > 0) optionstr[searchdrv-1] = STR_OPEN_CONTAINING_FOLDER;
    if (titleman > 0) optionstr[titleman-1] = STR_OPEN_TITLE_FOLDER;

    int user_select = (int) ((n_marked > 1) ?
        ShowSelectPrompt(n_opt, optionstr, STR_PATH_N_FILES_SELECTED, pathstr, n_marked) :
        ShowSelectPrompt(n_opt, optionstr, "%s%s", pathstr, tidstr));
    if (user_select == hexviewer) { // -> show in hex viewer
        FileHexViewer(file_path);
        GetDirContents(current_dir, current_path);
        return 0;
    }
    else if (user_select == textviewer) { // -> show in text viewer
        FileTextViewer(file_path, scriptable);
        return 0;
    }
    else if (user_select == calcsha256) { // -> calculate SHA-256
        ShaCalculator(file_path, false);
        GetDirContents(current_dir, current_path);
        return 0;
    }
    else if (user_select == calcsha1) { // -> calculate SHA-1
        ShaCalculator(file_path, true);
        GetDirContents(current_dir, current_path);
        return 0;
    }
    else if (user_select == calccmac) { // -> calculate CMAC
        optionstr[0] = STR_CHECK_CURRENT_CMAC_ONLY;
        optionstr[1] = STR_VERIFY_CMAC_FOR_ALL;
        optionstr[2] = STR_FIX_CMAC_FOR_ALL;
        user_select = (n_marked > 1) ? ShowSelectPrompt(3, optionstr, STR_PATH_N_FILES_SELECTED, pathstr, n_marked) : 1;
        if (user_select == 1) {
            CmacCalculator(file_path);
            return 0;
        } else if ((user_select == 2) || (user_select == 3)) {
            bool fix = (user_select == 3);
            u32 n_processed = 0;
            u32 n_success = 0;
            u32 n_fixed = 0;
            u32 n_nocmac = 0;
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                const char* path = current_dir->entry[i].path;
                if (!current_dir->entry[i].marked) continue;
                if (!ShowProgress(n_processed++, n_marked, path)) break;
                if (CheckCmacPath(path) != 0) {
                    n_nocmac++;
                    continue;
                }
                if (CheckFileCmac(path) == 0) n_success++;
                else if (fix && (FixFileCmac(path, true) == 0)) n_fixed++;
                else { // on failure: set cursor on failed file
                    *cursor = i;
                    continue;
                }
                current_dir->entry[i].marked = false;
            }
            if (n_fixed) {
                if (n_nocmac) ShowPrompt(false, STR_N_N_N_FILES_OK_FIXED_TOTAL_N_OF_N_HAVE_NO_CMAC,
                    n_success, n_fixed, n_marked, n_nocmac, n_marked);
                 else ShowPrompt(false, STR_N_OF_N_FILES_VERIFIED_N_OF_N_FILES_FIXED,
                    n_success, n_marked, n_fixed, n_marked);
            } else {
                if (n_nocmac) ShowPrompt(false, STR_N_OF_N_FILES_VERIFIED_N_OF_N_HAVE_NO_CMAC,
                    n_success, n_marked, n_nocmac, n_marked);
                else ShowPrompt(false, STR_N_OF_N_FILES_VERIFIED, n_success, n_marked);
            }
            return 0;
        }
        return FileHandlerMenu(current_path, cursor, scroll, pane);
    }
    else if (user_select == fileinfo) { // -> show file info
        DirFileAttrMenu(file_path, file_name);
        return 0;
    }
    else if (user_select == copystd) { // -> copy to OUTPUT_PATH
        StandardCopy(cursor, scroll);
        return 0;
    }
    else if (user_select == inject) { // -> inject data from clipboard
        char origstr[UTF_BUFFER_BYTESIZE(18)];
        TruncateString(origstr, clipboard->entry[0].name, 18, 10);
        u64 offset = ShowHexPrompt(0, 8, STR_INJECT_DATA_FROM_SPECIFY_OFFSET_BELOW, origstr);
        if (offset != (u64) -1) {
            if (!FileInjectFile(file_path, clipboard->entry[0].path, (u32) offset, 0, 0, NULL))
                ShowPrompt(false, STR_FAILED_INJECTING_PATH, origstr);
            clipboard->n_entries = 0;
        }
        return 0;
    }
    else if ((user_select == searchdrv) || (user_select == titleman)) { // -> open containing path
        char temp_path[256];
        if (user_select == searchdrv) strncpy(temp_path, file_path, 256);
        else if (GetTieContentPath(temp_path, file_path) != 0) return 0;

        char* last_slash = strrchr(temp_path, '/');
        if (last_slash) {
            if (N_PANES) { // switch to next pane
                memcpy((*pane)->path, current_path, 256);  // store current pane state
                (*pane)->cursor = *cursor;
                (*pane)->scroll = *scroll;
                if (++*pane >= panedata + N_PANES) *pane -= N_PANES;
            }
            snprintf(current_path, last_slash - temp_path + 1, "%s", temp_path);
            GetDirContents(current_dir, current_path);
            *scroll = 0;
            for (*cursor = 1; *cursor < current_dir->n_entries; (*cursor)++) {
                DirEntry* entry = &(current_dir->entry[*cursor]);
                if (strncasecmp(entry->path, temp_path, 256) == 0) break;
            }
            if (*cursor >= current_dir->n_entries)
                *cursor = 1;
        }
        return 0;
    }
    else if (user_select != special) {
        return 1;
    }

    // stuff for special menu starts here
    n_opt = 0;
    int show_info = (titleinfo) ? ++n_opt : -1;
    int mount = (mountable) ? ++n_opt : -1;
    int restore = (restorable) ? ++n_opt : -1;
    int ebackup = (ebackupable) ? ++n_opt : -1;
    int ncsdfix = (ncsdfixable) ? ++n_opt : -1;
    int decrypt = (decryptable) ? ++n_opt : -1;
    int encrypt = (encryptable) ? ++n_opt : -1;
    int cia_build = (cia_buildable) ? ++n_opt : -1;
    int cia_build_legit = (cia_buildable_legit) ? ++n_opt : -1;
    int cxi_dump = (cxi_dumpable) ? ++n_opt : -1;
    int cia_install = (cia_installable) ? ++n_opt : -1;
    int tik_install = (tik_installable) ? ++n_opt : -1;
    int tik_dump = (tik_dumpable) ? ++n_opt : -1;
    int cif_install = (cif_installable) ? ++n_opt : -1;
    int uninstall = (uninstallable) ? ++n_opt : -1;
    int tik_build_enc = (tik_buildable) ? ++n_opt : -1;
    int tik_build_dec = (tik_buildable) ? ++n_opt : -1;
    int key_build = (key_buildable) ? ++n_opt : -1;
    int verify = (verificable) ? ++n_opt : -1;
    int ctrtransfer = (transferable) ? ++n_opt : -1;
    int hsinject = (hsinjectable) ? ++n_opt : -1;
    int extrcode = (extrcodeable) ? ++n_opt : -1;
    int trim = (trimable) ? ++n_opt : -1;
    int rename = (renamable) ? ++n_opt : -1;
    int xorpad = (xorpadable) ? ++n_opt : -1;
    int xorpad_inplace = (xorpadable) ? ++n_opt : -1;
    int keyinit = (keyinitable) ? ++n_opt : -1;
    int keyinstall = (keyinstallable) ? ++n_opt : -1;
    int install = (installable) ? ++n_opt : -1;
    int boot = (bootable) ? ++n_opt : -1;
    int script = (scriptable) ? ++n_opt : -1;
    int luascript = (luascriptable) ? ++n_opt : -1;
    int font = (fontable) ? ++n_opt : -1;
    int translation = (translationable) ? ++n_opt : -1;
    int view = (viewable) ? ++n_opt : -1;
    int agbexport = (agbexportable) ? ++n_opt : -1;
    int agbimport = (agbimportable) ? ++n_opt : -1;
    int setup = (setable) ? ++n_opt : -1;
    if (mount > 0) optionstr[mount-1] = (filetype & GAME_TMD) ? STR_MOUNT_CXI_NDS_TO_DRIVE : STR_MOUNT_IMAGE_TO_DRIVE;
    if (restore > 0) optionstr[restore-1] = STR_RESTORE_SYSNAND_SAFE;
    if (ebackup > 0) optionstr[ebackup-1] = STR_UPDATE_EMBEDDED_BACKUP;
    if (ncsdfix > 0) optionstr[ncsdfix-1] = STR_REBUILD_NCSD_HEADER;
    if (show_info > 0) optionstr[show_info-1] = STR_SHOW_TITLE_INFO;
    if (decrypt > 0) optionstr[decrypt-1] = (cryptable_inplace) ? STR_DECRYPT_FILE : decryptto_str;
    if (encrypt > 0) optionstr[encrypt-1] = (cryptable_inplace) ? STR_ENCRYPT_FILE : encryptto_str;
    if (cia_build > 0) optionstr[cia_build-1] = (cia_build_legit < 0) ? STR_BUILD_CIA_FROM_FILE : STR_BUILD_CIA_STANDARD;
    if (cia_build_legit > 0) optionstr[cia_build_legit-1] = STR_BUILD_CIA_LEGIT;
    if (cxi_dump > 0) optionstr[cxi_dump-1] = STR_DUMP_CXI_NDS_FILE;
    if (cia_install > 0) optionstr[cia_install-1] = STR_INSTALL_GAME_IMAGE;
    if (tik_install > 0) optionstr[tik_install-1] = STR_INSTALL_TICKET;
    if (tik_dump > 0) optionstr[tik_dump-1] = STR_DUMP_TICKET_FILE;
    if (cif_install > 0) optionstr[cif_install-1] = STR_INSTALL_CIFINISH_BIN;
    if (uninstall > 0) optionstr[uninstall-1] = STR_UNINSTALL_TITLE;
    if (tik_build_enc > 0) optionstr[tik_build_enc-1] = buildtikdbenc_str;
    if (tik_build_dec > 0) optionstr[tik_build_dec-1] = buildtikdbdec_str;
    if (key_build > 0) optionstr[key_build-1] = buildkeydb_str;
    if (verify > 0) optionstr[verify-1] = STR_VERIFY_FILE;
    if (ctrtransfer > 0) optionstr[ctrtransfer-1] = STR_TRANSFER_IMAGE_TO_CTRNAND;
    if (hsinject > 0) optionstr[hsinject-1] = STR_INJECT_TO_H_AND_S;
    if (trim > 0) optionstr[trim-1] = STR_TRIM_FILE;
    if (rename > 0) optionstr[rename-1] = STR_RENAME_FILE;
    if (xorpad > 0) optionstr[xorpad-1] = STR_BUILD_XORPADS_SD;
    if (xorpad_inplace > 0) optionstr[xorpad_inplace-1] = STR_BUILD_XORPADS_INPLACE;
    if (extrcode > 0) optionstr[extrcode-1] = extractexefs_str;
    if (keyinit > 0) optionstr[keyinit-1] = initkeydb_str;
    if (keyinstall > 0) optionstr[keyinstall-1] = installkeydb_str;
    if (install > 0) optionstr[install-1] = STR_INSTALL_FIRM;
    if (boot > 0) optionstr[boot-1] = STR_BOOT_FIRM;
    if (script > 0) optionstr[script-1] = STR_EXECUTE_GM9_SCRIPT;
    if (luascript > 0) optionstr[luascript-1] = STR_EXECUTE_LUA_SCRIPT;
    if (view > 0) optionstr[view-1] = STR_VIEW_PNG_FILE;
    if (font > 0) optionstr[font-1] = STR_SET_AS_ACTIVE_FONT;
    if (translation > 0) optionstr[translation-1] = STR_SET_AS_ACTIVE_LANGUAGE;
    if (agbexport > 0) optionstr[agbexport-1] = STR_DUMP_BA_VC_SAVE;
    if (agbimport > 0) optionstr[agbimport-1] = STR_INJECT_GBA_VC_SAVE;
    if (setup > 0) optionstr[setup-1] = STR_SET_AS_DEFAULT;

    // auto select when there is only one option
    user_select = (n_opt <= 1) ? n_opt : (int) ((n_marked > 1) ?
        ShowSelectPrompt(n_opt, optionstr, STR_PATH_N_FILES_SELECTED, pathstr, n_marked) :
        ShowSelectPrompt(n_opt, optionstr, "%s%s", pathstr, tidstr));
    if (user_select == mount) { // -> mount file as image
        const char* mnt_drv_paths[] = { "7:", "G:", "K:", "T:", "I:", "D:" }; // maybe move that to fsdrive.h
        if (clipboard->n_entries && (DriveType(clipboard->entry[0].path) & DRV_IMAGE))
            clipboard->n_entries = 0; // remove last mounted image clipboard entries
        SetTitleManagerMode(false); // disable title manager mode
        InitImgFS((filetype & GAME_TMD) ? cxi_path : file_path);

        const char* drv_path = NULL; // find path of mounted drive
        for (u32 i = 0; i < (sizeof(mnt_drv_paths) / sizeof(const char*)); i++) {
            if (DriveType((drv_path = mnt_drv_paths[i]))) break;
            drv_path = NULL;
        }

        if (!drv_path) {
            ShowPrompt(false, "%s", STR_MOUNTING_IMAGE_FAILED);
            InitImgFS(NULL);
        } else { // open in next pane?
            if (ShowPrompt(true, STR_PATH_MOUNTED_AS_DRIVE_ENTER_PATH_NOW, pathstr, drv_path)) {
                if (N_PANES) {
                    memcpy((*pane)->path, current_path, 256);  // store current pane state
                    (*pane)->cursor = *cursor;
                    (*pane)->scroll = *scroll;
                    if (++*pane >= panedata + N_PANES) *pane -= N_PANES;
                }

                strncpy(current_path, drv_path, 256);
                GetDirContents(current_dir, current_path);
                *cursor = 1;
                *scroll = 0;
            }
        }
        return 0;
    }
    else if (user_select == decrypt) { // -> decrypt game file
        if (cryptable_inplace) {
            char decryptToOut[UTF_BUFFER_BYTESIZE(64)];
            snprintf(decryptToOut, sizeof(decryptToOut), STR_DECRYPT_TO_OUT, OUTPUT_PATH);
            optionstr[0] = decryptToOut;
            optionstr[1] = STR_DECRYPT_INPLACE;
            user_select = (int) ((n_marked > 1) ?
                ShowSelectPrompt(2, optionstr, STR_PATH_N_FILES_SELECTED, pathstr, n_marked) :
                ShowSelectPrompt(2, optionstr, "%s%s", pathstr, tidstr));
        } else user_select = 1;
        bool inplace = (user_select == 2);
        if (!user_select) { // do nothing when no choice is made
        } else if ((n_marked > 1) && ShowPrompt(true, STR_TRY_TO_DECRYPT_ALL_N_SELECTED_FILES, n_marked)) {
            u32 n_success = 0;
            u32 n_unencrypted = 0;
            u32 n_other = 0;
            ShowString(STR_TRYING_TO_DECRYPT_N_FILES, n_marked);
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                const char* path = current_dir->entry[i].path;
                if (!current_dir->entry[i].marked)
                    continue;
                if (!(IdentifyFileType(path) & filetype & TYPE_BASE)) {
                    n_other++;
                    continue;
                }
                if (!(filetype & BIN_KEYDB) && (CheckEncryptedGameFile(path) != 0)) {
                    n_unencrypted++;
                    continue;
                }
                DrawDirContents(current_dir, (*cursor = i), scroll);
                if (!(filetype & BIN_KEYDB) && (CryptGameFile(path, inplace, false) == 0)) n_success++;
                else if ((filetype & BIN_KEYDB) && (CryptAesKeyDb(path, inplace, false) == 0)) n_success++;
                else { // on failure: show error, continue
                    char lpathstr[UTF_BUFFER_BYTESIZE(32)];
                    TruncateString(lpathstr, path, 32, 8);
                    if (ShowPrompt(true, "%s\n%s", lpathstr, STR_DECRYPTION_FAILED_CONTINUE)) continue;
                    else break;
                }
                current_dir->entry[i].marked = false;
            }
            if (n_other || n_unencrypted) {
                ShowPrompt(false, STR_N_OF_N_FILES_DECRYPTED_N_OF_N_NOT_ENCRYPTED_N_OF_N_NOT_SAME_TYPE,
                    n_success, n_marked, n_unencrypted, n_marked, n_other, n_marked);
            } else ShowPrompt(false, STR_N_OF_N_FILES_DECRYPTED, n_success, n_marked);
            if (!inplace && n_success) ShowPrompt(false, STR_N_FILES_WRITTEN_TO_OUT, n_success, OUTPUT_PATH);
        } else {
            if (!(filetype & BIN_KEYDB) && (CheckEncryptedGameFile(file_path) != 0)) {
                ShowPrompt(false, "%s\n%s", pathstr, STR_FILE_NOT_ENCRYPTED);
            } else {
                u32 ret = (filetype & BIN_KEYDB) ? CryptAesKeyDb(file_path, inplace, false) :
                    CryptGameFile(file_path, inplace, false);
                if (inplace || (ret != 0)) ShowPrompt(false, "%s\n%s", pathstr, (ret == 0) ? STR_DECRYPTION_SUCCESS : STR_DECRYPTION_FAILED);
                else ShowPrompt(false, STR_PATH_DECRYPTED_TO_OUT, pathstr, OUTPUT_PATH);
            }
        }
        return 0;
    }
    else if (user_select == encrypt) { // -> encrypt game file
        if (cryptable_inplace) {
            char encryptToOut[UTF_BUFFER_BYTESIZE(64)];
            snprintf(encryptToOut, sizeof(encryptToOut), STR_ENCRYPT_TO_OUT, OUTPUT_PATH);
            optionstr[0] = encryptToOut;
            optionstr[1] = STR_ENCRYPT_INPLACE;
            user_select = (int) ((n_marked > 1) ?
                ShowSelectPrompt(2, optionstr, STR_PATH_N_FILES_SELECTED, pathstr, n_marked) :
                ShowSelectPrompt(2, optionstr, "%s%s", pathstr, tidstr));
        } else user_select = 1;
        bool inplace = (user_select == 2);
        if (!user_select) { // do nothing when no choice is made
        } else if ((n_marked > 1) && ShowPrompt(true, STR_TRY_TO_ENCRYPT_N_SELECTED_FILES, n_marked)) {
            u32 n_success = 0;
            u32 n_other = 0;
            ShowString(STR_TRYING_TO_ENCRYPT_N_FILES, n_marked);
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                const char* path = current_dir->entry[i].path;
                if (!current_dir->entry[i].marked)
                    continue;
                if (!(IdentifyFileType(path) & filetype & TYPE_BASE)) {
                    n_other++;
                    continue;
                }
                DrawDirContents(current_dir, (*cursor = i), scroll);
                if (!(filetype & BIN_KEYDB) && (CryptGameFile(path, inplace, true) == 0)) n_success++;
                else if ((filetype & BIN_KEYDB) && (CryptAesKeyDb(path, inplace, true) == 0)) n_success++;
                else { // on failure: show error, continue
                    char lpathstr[UTF_BUFFER_BYTESIZE(32)];
                    TruncateString(lpathstr, path, 32, 8);
                    if (ShowPrompt(true, "%s\n%s", lpathstr, STR_ENCRYPTION_FAILED_CONTINUE)) continue;
                    else break;
                }
                current_dir->entry[i].marked = false;
            }
            if (n_other) {
                ShowPrompt(false, STR_N_OF_N_FILES_ENCRYPTED_N_OF_N_NOT_SAME_TYPE,
                    n_success, n_marked, n_other, n_marked);
            } else ShowPrompt(false, STR_N_OF_N_FILES_ENCRYPTED, n_success, n_marked);
            if (!inplace && n_success) ShowPrompt(false, STR_N_FILES_WRITTEN_TO_OUT, n_success, OUTPUT_PATH);
        } else {
            u32 ret = (filetype & BIN_KEYDB) ? CryptAesKeyDb(file_path, inplace, true) :
                CryptGameFile(file_path, inplace, true);
            if (inplace || (ret != 0)) ShowPrompt(false, "%s\n%s", pathstr, (ret == 0) ? STR_ENCRYPTION_SUCCESS : STR_ENCRYPTION_FAILED);
            else ShowPrompt(false, STR_PATH_ENCRYPTED_TO_OUT, pathstr, OUTPUT_PATH);
        }
        return 0;
    }
    else if ((user_select == cia_build) || (user_select == cia_build_legit) || (user_select == cxi_dump)) { // -> build CIA / dump CXI
        char* type = (user_select == cxi_dump) ? "CXI" : "CIA";
        bool force_legit = (user_select == cia_build_legit);
        if ((n_marked > 1) && ShowPrompt(true, STR_TRY_TO_PROCESS_N_SELECTED_FILES, n_marked)) {
            u32 n_success = 0;
            u32 n_other = 0;
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                const char* path = current_dir->entry[i].path;
                if (!current_dir->entry[i].marked)
                    continue;
                if (!(IdentifyFileType(path) & filetype & TYPE_BASE)) {
                    n_other++;
                    continue;
                }
                DrawDirContents(current_dir, (*cursor = i), scroll);
                if (((user_select != cxi_dump) && (BuildCiaFromGameFile(path, force_legit) == 0)) ||
                    ((user_select == cxi_dump) && (DumpCxiSrlFromGameFile(path) == 0))) n_success++;
                else { // on failure: show error, continue
                    char lpathstr[UTF_BUFFER_BYTESIZE(32)];
                    TruncateString(lpathstr, path, 32, 8);
                    if (ShowPrompt(true, STR_PATH_BUILD_TYPE_FAILED_CONTINUE, lpathstr, type)) continue;
                    else break;
                }
                current_dir->entry[i].marked = false;
            }
            if (n_other) ShowPrompt(false, STR_N_OF_N_TYPES_BUILT_N_OF_N_NOT_SAME_TYPE,
                n_success, n_marked, type, n_other, n_marked);
            else ShowPrompt(false, STR_N_OF_N_TYPES_BUILT, n_success, n_marked, type);
            if (n_success) ShowPrompt(false, STR_N_FILES_WRITTEN_TO_OUT, n_success, OUTPUT_PATH);
            if (n_success && in_output_path) GetDirContents(current_dir, current_path);
            if (n_success != (n_marked - n_other)) {
                ShowPrompt(false, STR_N_FILES_FAILED_CONVERTION_VERIFICATION_RECOMMENDED,
                    n_marked - (n_success + n_other));
            }
        } else {
            if (((user_select != cxi_dump) && (BuildCiaFromGameFile(file_path, force_legit) == 0)) ||
                ((user_select == cxi_dump) && (DumpCxiSrlFromGameFile(file_path) == 0))) {
                ShowPrompt(false, STR_PATH_TYPE_BUILT_TO_OUT, pathstr, type, OUTPUT_PATH);
                if (in_output_path) GetDirContents(current_dir, current_path);
            } else {
                ShowPrompt(false, STR_PATH_TYPE_BUILD_FAILED, pathstr, type);
                if ((filetype & (GAME_NCCH|GAME_NCSD)) &&
                    ShowPrompt(true, "%s\n%s", pathstr, STR_FILE_FAILED_CONVERSION_VERIFY_NOW)) {
                    ShowPrompt(false, "%s\n%s", pathstr, (VerifyGameFile(file_path) == 0) ? STR_VERIFICATION_SUCCESS : STR_VERIFICATION_FAILED);
                }
            }
        }
        return 0;
    }
    else if ((user_select == cia_install) || (user_select == tik_install) ||
             (user_select == cif_install)) { // -> install game/ticket/cifinish file
        u32 (*InstallFunction)(const char*, bool) =
            (user_select == cia_install) ? &InstallGameFile :
            (user_select == tik_install) ? &InstallTicketFile : &InstallCifinishFile;
        bool to_emunand = false;
        if (CheckVirtualDrive("E:")) {
            optionstr[0] = STR_INSTALL_TO_SYSNAND;
            optionstr[1] = STR_INSTALL_TO_EMUNAND;
            user_select = (int) ((n_marked > 1) ?
                ShowSelectPrompt(2, optionstr, STR_PATH_N_FILES_SELECTED, pathstr, n_marked) :
                ShowSelectPrompt(2, optionstr, "%s%s", pathstr, tidstr));
            if (!user_select) return 0;
            else to_emunand = (user_select == 2);
        }
        if ((n_marked > 1) && ShowPrompt(true, STR_TRY_TO_INSTALL_N_SELECTED_FILES, n_marked)) {
            u32 n_success = 0;
            u32 n_other = 0;
            ShowString(STR_TRYING_TO_INSTALL_N_FILES, n_marked);
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                const char* path = current_dir->entry[i].path;
                if (!current_dir->entry[i].marked)
                    continue;
                if (!(IdentifyFileType(path) & filetype & TYPE_BASE)) {
                    n_other++;
                    continue;
                }
                DrawDirContents(current_dir, (*cursor = i), scroll);
                if ((*InstallFunction)(path, to_emunand) == 0)
                    n_success++;
                else { // on failure: show error, continue
                    char lpathstr[UTF_BUFFER_BYTESIZE(32)];
                    TruncateString(lpathstr, path, 32, 8);
                    if (ShowPrompt(true, "%s\n%s", lpathstr, STR_INSTALL_FAILED_CONTINUE)) continue;
                    else break;
                }
                current_dir->entry[i].marked = false;
            }
            if (n_other) {
                ShowPrompt(false, STR_N_OF_N_FILES_INSTALLED_N_OF_N_NOT_SAME_TYPE,
                    n_success, n_marked, n_other, n_marked);
            } else ShowPrompt(false, STR_N_OF_N_FILES_INSTALLED, n_success, n_marked);
        } else {
            u32 ret = (*InstallFunction)(file_path, to_emunand);
            ShowPrompt(false, "%s\n%s", pathstr, (ret == 0) ? STR_INSTALL_SUCCESS : STR_INSTALL_FAILED);
            if ((ret != 0) && (filetype & (GAME_NCCH|GAME_NCSD)) &&
                ShowPrompt(true, "%s\n%s", pathstr, STR_FILE_FAILED_INSTALL_VERIFY_NOW)) {
                ShowPrompt(false, "%s\n%s", pathstr, (VerifyGameFile(file_path) == 0) ? STR_VERIFICATION_SUCCESS : STR_VERIFICATION_FAILED);
            }
        }
        return 0;
    }
    else if (user_select == uninstall) { // -> uninstall title
        bool full_uninstall = false;

        // safety confirmation
        optionstr[0] = STR_KEEP_TICKET_AND_SAVEGAME;
        optionstr[1] = STR_UNINSTALL_EVERYTHING;
        optionstr[2] = STR_ABORT_UNINSTALL;
        user_select = (int) (n_marked > 1) ?
            ShowSelectPrompt(3, optionstr, STR_UNINSTALL_N_SELECTED_TITLES, n_marked) :
            ShowSelectPrompt(3, optionstr, "%s\n%s", pathstr, STR_UNINSTALL_SELECTED_TITLE);
        full_uninstall = (user_select == 2);
        if (!user_select || (user_select == 3))
            return 0;

        // batch uninstall
        if (n_marked > 1) {
            u32 n_success = 0;
            u32 num = 0;
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                const char* path = current_dir->entry[i].path;
                if (!current_dir->entry[i].marked) continue;
                if (!(IdentifyFileType(path) & filetype & TYPE_BASE)) continue;
                if (!num && !CheckWritePermissions(path)) break;
                if (!ShowProgress(num++, n_marked, path)) break;
                if (UninstallGameDataTie(path, true, full_uninstall, full_uninstall) == 0)
                    n_success++;
            }
            ShowPrompt(false, STR_N_OF_N_TITLES_UNINSTALLED, n_success, n_marked);
        } else if (CheckWritePermissions(file_path)) {
            ShowString("%s\n%s", pathstr, STR_UNINSTALLING_PLEASE_WAIT);
            if (UninstallGameDataTie(file_path, true, full_uninstall, full_uninstall) != 0)
                ShowPrompt(false, "%s\n%s", pathstr, STR_UNINSTALL_FAILED);
            ClearScreenF(true, false, COLOR_STD_BG);
        }

        GetDirContents(current_dir, current_path);
        return 0;
    }
    else if (user_select == verify) { // -> verify game / nand file
        if ((n_marked > 1) && ShowPrompt(true, STR_TRY_TO_VERIFY_N_SELECTED_FILES, n_marked)) {
            u32 n_success = 0;
            u32 n_other = 0;
            u32 n_processed = 0;
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                const char* path = current_dir->entry[i].path;
                if (!current_dir->entry[i].marked)
                    continue;
                if (!(filetype & (GAME_CIA|GAME_TMD|GAME_NCSD|GAME_NCCH)) &&
                    !ShowProgress(n_processed++, n_marked, path)) break;
                if (!(IdentifyFileType(path) & filetype & TYPE_BASE)) {
                    n_other++;
                    continue;
                }
                DrawDirContents(current_dir, (*cursor = i), scroll);
                if ((filetype & IMG_NAND) && (ValidateNandDump(path) == 0)) n_success++;
                else if (VerifyGameFile(path) == 0) n_success++;
                else { // on failure: show error, continue
                    char lpathstr[UTF_BUFFER_BYTESIZE(32)];
                    TruncateString(lpathstr, path, 32, 8);
                    if (ShowPrompt(true, "%s\n%s", lpathstr, STR_VERIFICATION_FAILED_CONTINUE)) {
                        if (!(filetype & (GAME_CIA|GAME_TMD|GAME_NCSD|GAME_NCCH)))
                            ShowProgress(0, n_marked, path); // restart progress bar
                        continue;
                    } else break;
                }
                current_dir->entry[i].marked = false;
            }
            if (n_other) ShowPrompt(false, STR_N_OF_N_FILES_VERIFIED_N_OF_N_NOT_SAME_TYPE,
                n_success, n_marked, n_other, n_marked);
            else ShowPrompt(false, STR_N_OF_N_FILES_VERIFIED, n_success, n_marked);
        } else {
            ShowString("%s\n%s", pathstr, STR_VERIFYING_FILE_PLEASE_WAIT);
            if (filetype & IMG_NAND) {
                ShowPrompt(false, "%s\n%s", pathstr, (ValidateNandDump(file_path) == 0) ? STR_NAND_VALIDATION_SUCCESS : STR_NAND_VALIDATION_FAILED);
            } else ShowPrompt(false, "%s\n%s", pathstr, (VerifyGameFile(file_path) == 0) ? STR_VERIFICATION_SUCCESS : STR_VERIFICATION_FAILED);
        }
        return 0;
    }
    else if (user_select == tik_dump) { // dump ticket file
        if ((n_marked > 1) && ShowPrompt(true, STR_DUMP_FOR_N_SELECTED_FILES, n_marked)) {
            u32 n_success = 0;
            u32 n_legit = 0;
            bool force_legit = true;
            for (u32 n_processed = 0;; n_processed = 0) {
                for (u32 i = 0; i < current_dir->n_entries; i++) {
                    const char* path = current_dir->entry[i].path;
                    if (!current_dir->entry[i].marked) continue;
                    if (!ShowProgress(n_processed++, n_marked, path)) break;
                    DrawDirContents(current_dir, (*cursor = i), scroll);
                    if (DumpTicketForGameFile(path, force_legit) == 0) n_success++;
                    else if (IdentifyFileType(path) & filetype & TYPE_BASE) continue;
                    if (force_legit) n_legit++;
                    current_dir->entry[i].marked = false;
                }
                if (force_legit && (n_success != n_marked))
                    if (!ShowPrompt(true, STR_N_OF_N_LEGIT_TICKETS_DUMPED_ATTEMPT_DUMP_ALL, n_legit, n_marked)) break;
                if (!force_legit) break;
                force_legit = false;
            }
            ShowPrompt(false, STR_N_OF_N_TICKETS_DUMPED_TO_OUT, n_success, n_marked, OUTPUT_PATH);
        } else {
            if (DumpTicketForGameFile(file_path, true) == 0) {
                ShowPrompt(false, STR_PATH_TICKET_DUMPED_TO_OUT, pathstr, OUTPUT_PATH);
            } else if (ShowPrompt(false, STR_LEGIT_TICKET_NOT_FOUND_DUMP_ANYWAYS, pathstr)) {
                if (DumpTicketForGameFile(file_path, false) == 0)
                    ShowPrompt(false, STR_PATH_TICKET_DUMPED_TO_OUT, pathstr, OUTPUT_PATH);
                else ShowPrompt(false, "%s\n%s", pathstr, STR_DUMP_TICKET_FAILED);
            }
        }
        return 0;
    }
    else if ((user_select == tik_build_enc) || (user_select == tik_build_dec)) { // -> (re)build titlekey database
        bool dec = (user_select == tik_build_dec);
        const char* path_out = (dec) ? OUTPUT_PATH "/" TIKDB_NAME_DEC : OUTPUT_PATH "/" TIKDB_NAME_ENC;
        if (BuildTitleKeyInfo(NULL, dec, false) != 0) return 1; // init database
        ShowString(STR_BUILDING_X, (dec) ? TIKDB_NAME_DEC : TIKDB_NAME_ENC);
        if (n_marked > 1) {
            u32 n_success = 0;
            u32 n_other = 0;
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                const char* path = current_dir->entry[i].path;
                if (!current_dir->entry[i].marked)
                    continue;
                if (!FTYPE_TIKBUILD(IdentifyFileType(path))) {
                    n_other++;
                    continue;
                }
                current_dir->entry[i].marked = false;
                if (BuildTitleKeyInfo(path, dec, false) == 0) n_success++; // ignore failures for now
            }
            if (BuildTitleKeyInfo(NULL, dec, true) == 0) {
                if (n_other) ShowPrompt(false, STR_PATH_N_OF_N_FILES_PROCESSED_N_OF_N_FILES_IGNORED,
                    path_out, n_success, n_marked, n_other, n_marked);
                else ShowPrompt(false, STR_PATH_N_OF_N_FILES_PROCESSED, path_out, n_success, n_marked);
            } else ShowPrompt(false, "%s\n%s", path_out, STR_BUILD_DATABASE_FAILED);
        } else ShowPrompt(false, "%s\n%s", path_out, (BuildTitleKeyInfo(file_path, dec, true) == 0) ? STR_BUILD_DATABASE_SUCCESS : STR_BUILD_DATABASE_FAILED);
        return 0;
    }
    else if (user_select == key_build) { // -> (Re)Build AES key database
        const char* path_out = OUTPUT_PATH "/" KEYDB_NAME;
        if (BuildKeyDb(NULL, false) != 0) return 1; // init database
        ShowString(STR_BUILDING_X, KEYDB_NAME);
        if (n_marked > 1) {
            u32 n_success = 0;
            u32 n_other = 0;
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                const char* path = current_dir->entry[i].path;
                if (!current_dir->entry[i].marked)
                    continue;
                if (!FTYPE_KEYBUILD(IdentifyFileType(path))) {
                    n_other++;
                    continue;
                }
                current_dir->entry[i].marked = false;
                if (BuildKeyDb(path, false) == 0) n_success++; // ignore failures for now
            }
            if (BuildKeyDb(NULL, true) == 0) {
                if (n_other) ShowPrompt(false, STR_PATH_N_OF_N_FILES_PROCESSED_N_OF_N_FILES_IGNORED,
                    path_out, n_success, n_marked, n_other, n_marked);
                else ShowPrompt(false, STR_PATH_N_OF_N_FILES_PROCESSED, path_out, n_success, n_marked);
            } else ShowPrompt(false, "%s\n%s", path_out, STR_BUILD_DATABASE_FAILED);
        } else ShowPrompt(false, "%s\n%s", path_out, (BuildKeyDb(file_path, true) == 0) ? STR_BUILD_DATABASE_SUCCESS : STR_BUILD_DATABASE_FAILED);
        return 0;
    }
    else if (user_select == trim) { // -> Game file trimmer
        if ((n_marked > 1) && ShowPrompt(true, STR_TRY_TO_TRIM_N_SELECTED_FILES, n_marked)) {
            u32 n_success = 0;
            u32 n_other = 0;
            u32 n_processed = 0;
            u64 savings = 0;
            char savingsstr[32];
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                const char* path = current_dir->entry[i].path;
                u64 prevsize = 0;
                if (!current_dir->entry[i].marked)
                    continue;
                if (!ShowProgress(n_processed++, n_marked, path)) break;
                if (!(IdentifyFileType(path) & filetype & TYPE_BASE)) {
                    n_other++;
                    continue;
                }
                prevsize = FileGetSize(path);
                if (TrimGameFile(path) == 0) {
                    n_success++;
                    savings += prevsize - FileGetSize(path);
                } else { // on failure: show error, continue (should not happen)
                    char lpathstr[UTF_BUFFER_BYTESIZE(32)];
                    TruncateString(lpathstr, path, 32, 8);
                    if (ShowPrompt(true, "%s\n%s", lpathstr, STR_TRIMMING_FAILED_CONTINUE)) {
                        ShowProgress(0, n_marked, path); // restart progress bar
                        continue;
                    } else break;
                }
                current_dir->entry[i].marked = false;
            }
            FormatBytes(savingsstr, savings);
            if (n_other) ShowPrompt(false, STR_N_OF_N_FILES_TRIMMED_N_OF_N_NOT_OF_SAME_TYPE_X_SAVED,
                n_success, n_marked, n_other, n_marked, savingsstr);
            else ShowPrompt(false, STR_N_OF_N_FILES_TRIMMED_X_SAVED, n_success, n_marked, savingsstr);
            if (n_success) GetDirContents(current_dir, current_path);
        } else {
            u64 trimsize = GetGameFileTrimmedSize(file_path);
            u64 currentsize = FileGetSize(file_path);
            char tsizestr[32];
            char csizestr[32];
            char dsizestr[32];
            FormatBytes(tsizestr, trimsize);
            FormatBytes(csizestr, currentsize);
            FormatBytes(dsizestr, currentsize - trimsize);

            if (!trimsize || trimsize > currentsize) {
                ShowPrompt(false, "%s\n%s", pathstr, STR_FILE_CANT_BE_TRIMMED);
            } else if (trimsize == currentsize) {
                ShowPrompt(false, "%s\n%s", pathstr, STR_FILE_ALREADY_TRIMMED);
            } else if (ShowPrompt(true, STR_PATH_CURRENT_SIZE_TRIMMED_SIZE_DIFFERENCE_TRIM_FILE,
                pathstr, csizestr, tsizestr, dsizestr)) {
                if (TrimGameFile(file_path) != 0) ShowPrompt(false, "%s\n%s", pathstr, STR_TRIMMING_FAILED);
                else {
                    ShowPrompt(false, STR_PATH_TRIMMED_BY_X, pathstr, dsizestr);
                    GetDirContents(current_dir, current_path);
                }
            }
        }
        return 0;
    }
    else if (user_select == rename) { // -> Game file renamer
        if ((n_marked > 1) && ShowPrompt(true, STR_TRY_TO_RENAME_N_SELECTED_FILES, n_marked)) {
            u32 n_success = 0;
            ShowProgress(0, 0, "");
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                DirEntry* entry = &(current_dir->entry[i]);
                if (!current_dir->entry[i].marked) continue;
                ShowProgress(i+1, current_dir->n_entries, entry->name);
                if (!GoodRenamer(entry, false)) continue;
                n_success++;
                current_dir->entry[i].marked = false;
            }
            ShowPrompt(false, STR_N_OF_N_RENAMED, n_success, n_marked);
        } else if (!GoodRenamer(&(current_dir->entry[*cursor]), true)) {
            ShowPrompt(false, "%s\n%s", pathstr, STR_COULD_NOT_RENAME_TO_GOOD_NAME);
        }
        return 0;
    }
    else if (user_select == show_info) { // -> Show title info
        ShowGameCheckerInfo(file_path);
        return 0;
    }
    else if (user_select == hsinject) { // -> Inject to Health & Safety
        char* destdrv[2] = { NULL };
        n_opt = 0;
        if (DriveType("1:")) {
            optionstr[n_opt] = STR_SYSNAND_H_AND_S_INJECT;
            destdrv[n_opt++] = "1:";
        }
        if (DriveType("4:")) {
            optionstr[n_opt] = STR_EMUNAND_H_AND_S_INJECT;
            destdrv[n_opt++] = "4:";
        }
        user_select = (n_opt > 1) ? (int) ShowSelectPrompt(n_opt, optionstr, "%s", pathstr) : n_opt;
        if (user_select) {
            ShowPrompt(false, "%s\n%s", pathstr, (InjectHealthAndSafety(file_path, destdrv[user_select-1]) == 0) ?
                STR_H_AND_S_INJECT_SUCCESS : STR_H_AND_S_INJECT_FAILURE);
        }
        return 0;
    }
    else if (user_select == extrcode) { // -> Extract .code
        if ((n_marked > 1) && ShowPrompt(true, STR_TRY_EXTRACT_ALL_N_SELECTED_FILES, n_marked)) {
            u32 n_success = 0;
            u32 n_other = 0;
            u32 n_processed = 0;
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                const char* path = current_dir->entry[i].path;
                if (!current_dir->entry[i].marked)
                    continue;
                if (!ShowProgress(n_processed++, n_marked, path)) break;
                if (!(IdentifyFileType(path) & filetype & TYPE_BASE)) {
                    n_other++;
                    continue;
                }
                DrawDirContents(current_dir, (*cursor = i), scroll);
                if (filetype & GAME_TMD) {
                    char cxi_pathl[256] = { 0 };
                    if ((GetTmdContentPath(cxi_pathl, path) == 0) && PathExist(cxi_pathl) &&
                        (ExtractCodeFromCxiFile(cxi_pathl, NULL, NULL) == 0)) {
                        n_success++;
                    } else continue;
                } else {
                    if (ExtractCodeFromCxiFile(path, NULL, NULL) == 0) n_success++;
                    else continue;
                }
                current_dir->entry[i].marked = false;
            }
            if (n_other) ShowPrompt(false, STR_N_OF_N_FILES_EXTRACTED_N_OF_N_NOT_SAME_TYPE,
                n_success, n_marked, n_other, n_marked);
            else ShowPrompt(false, STR_N_OF_N_FILES_EXTRACTED, n_success, n_marked);
        } else {
            char extstr[8] = { 0 };
            ShowString("%s\n%s", pathstr, STR_EXTRACTING_DOT_CODE);
            if (ExtractCodeFromCxiFile((filetype & GAME_TMD) ? cxi_path : file_path, NULL, extstr) == 0) {
                ShowPrompt(false, STR_PATH_EXT_EXTRACTED_TO_OUT, pathstr, extstr, OUTPUT_PATH);
            } else ShowPrompt(false, "%s\n%s", pathstr, STR_DOT_CODE_EXTRACT_FAILED);
        }
        return 0;
    }
    else if (user_select == ctrtransfer) { // -> transfer CTRNAND image to SysNAND
        char* destdrv[2] = { NULL };
        n_opt = 0;
        if (DriveType("1:")) {
            optionstr[n_opt] = STR_TRANSFER_TO_SYSNAND;
            destdrv[n_opt++] = "1:";
        }
        if (DriveType("4:")) {
            optionstr[n_opt] = STR_TRANSFER_TO_EMUNAND;
            destdrv[n_opt++] = "4:";
        }
        if (n_opt) {
            user_select = (n_opt > 1) ? (int) ShowSelectPrompt(n_opt, optionstr, "%s", pathstr) : 1;
            if (user_select) {
                ShowPrompt(false, "%s\n%s", pathstr, (TransferCtrNandImage(file_path, destdrv[user_select-1]) == 0) ?
                    STR_CTRNAND_TRANSFER_SUCCESS: STR_CTRNAND_TRANSFER_FAILED);
            }
        } else ShowPrompt(false, "%s\n%s", pathstr, STR_NO_VALID_DESTINATION_FOUND);
        return 0;
    }
    else if (user_select == restore) { // -> restore SysNAND (A9LH preserving)
        ShowPrompt(false, "%s\n%s", pathstr, (SafeRestoreNandDump(file_path) == 0) ?
            STR_NAND_RESTORE_SUCCESS : STR_NAND_RESTORE_FAILED);
        return 0;
    }
    else if (user_select == ncsdfix) { // -> inject sighaxed NCSD
        ShowPrompt(false, "%s\n%s", pathstr, (FixNandHeader(file_path, !(filetype == HDR_NAND)) == 0) ?
            STR_REBUILD_NCSD_SUCCESS : STR_REBUILD_NCSD_FAILED);
        GetDirContents(current_dir, current_path);
        InitExtFS(); // this might have fixed something, so try this
        return 0;
    }
    else if ((user_select == xorpad) || (user_select == xorpad_inplace)) { // -> build xorpads
        bool inplace = (user_select == xorpad_inplace);
        bool success = (BuildNcchInfoXorpads((inplace) ? current_path : OUTPUT_PATH, file_path) == 0);
        ShowPrompt(false, (success) ? STR_PATH_NCCHINFO_PADGEN_SUCCESS : STR_PATH_NCCHINFO_PADGEN_FAILED,
            pathstr, (!success || inplace) ? '\0' : '\n', OUTPUT_PATH);
        GetDirContents(current_dir, current_path);
        for (; *cursor < current_dir->n_entries; (*cursor)++) {
            DirEntry* entry = &(current_dir->entry[*cursor]);
            if (strncasecmp(entry->name, NCCHINFO_NAME, 32) == 0) break;
        }
        if (*cursor >= current_dir->n_entries) {
            *scroll = 0;
            *cursor = 1;
        }
        return 0;
    }
    else if (user_select == ebackup) { // -> update embedded backup
        ShowString("%s\n%s", pathstr, STR_UPDATING_EMBEDDED_BACKUP);
        bool required = (CheckEmbeddedBackup(file_path) != 0);
        bool success = (required && (EmbedEssentialBackup(file_path) == 0));
        ShowPrompt(false, "%s\n%s", pathstr, (!required) ? STR_BACKUP_UPDATE_NOT_REQUIRED :
            (success) ? STR_BACKUP_UPDATE_COMPLETED : STR_BACKUP_UPDATE_FAILED);
        GetDirContents(current_dir, current_path);
        return 0;
    }
    else if (user_select == keyinit) { // -> initialise keys from aeskeydb.bin
        if (ShowPrompt(true, "%s", STR_WARNING_KEYS_NOT_VERIFIED_CONTINUE_AT_YOUR_OWN_RISK))
            ShowPrompt(false, "%s\n%s", pathstr, (InitKeyDb(file_path) == 0) ?
                STR_AESKEYDB_INIT_SUCCESS : STR_AESKEYDB_INIT_FAILED);
        return 0;
    }
    else if (user_select == keyinstall) { // -> install keys from aeskeydb.bin
        ShowPrompt(false, "%s\n%s", pathstr, (SafeInstallKeyDb(file_path) == 0) ?
            STR_AESKEYDB_INSTALL_SUCCESS : STR_AESKEYDB_INSTALL_FAILED);
        return 0;
    }
    else if (user_select == install) { // -> install FIRM
        size_t firm_size = FileGetSize(file_path);
        u32 slots = 1;
        if (GetNandPartitionInfo(NULL, NP_TYPE_FIRM, NP_SUBTYPE_CTR, 1, NAND_SYSNAND) == 0) {
            optionstr[0] = STR_INSTALL_TO_FIRM0;
            optionstr[1] = STR_INSTALL_TO_FIRM1;
            optionstr[2] = STR_INSTALL_TO_BOTH;
            // this only works up to FIRM1
            slots = ShowSelectPrompt(3, optionstr, STR_PATH_N_KB_INSTALL_TO_SYSNAND, pathstr, firm_size / 1024);
        } else slots = ShowPrompt(true, STR_PATH_N_KB_INSTALL_TO_SYSNAND, pathstr, firm_size / 1024) ? 1 : 0;
        if (slots) ShowPrompt(false, (SafeInstallFirm(file_path, slots) == 0) ?
            STR_PATH_N_KB_INSTALL_SUCCESS : STR_PATH_N_KB_INSTALL_FAILED, pathstr, firm_size / 1024);
        return 0;
    }
    else if (user_select == boot) { // -> boot FIRM
        BootFirmHandler(file_path, true, false);
        return 0;
    }
    else if (user_select == script) { // execute script
        if (ShowPrompt(true, "%s\n%s", pathstr, STR_WARNING_DO_NOT_RUN_UNTRUSTED_SCRIPTS))
            ShowPrompt(false, "%s\n%s", pathstr, ExecuteGM9Script(file_path) ? STR_SCRIPT_EXECUTE_SUCCESS : STR_SCRIPT_EXECUTE_FAILURE);
        GetDirContents(current_dir, current_path);
        ClearScreenF(true, true, COLOR_STD_BG);
        return 0;
    }
    else if (user_select == luascript) { // execute lua script
#ifndef NO_LUA
        if (ShowPrompt(true, "%s\n%s", pathstr, STR_WARNING_DO_NOT_RUN_UNTRUSTED_SCRIPTS))
            ShowPrompt(false, "%s\n%s", pathstr, ExecuteLuaScript(file_path) ? STR_SCRIPT_EXECUTE_SUCCESS : STR_SCRIPT_EXECUTE_FAILURE);
#else
	// instead of outright removing the option, i think
	// having it display an error would be useful
	// so the user knows their issue exactly
	ShowPrompt(false, "%s", STR_LUA_NOT_INCLUDED);
#endif
        GetDirContents(current_dir, current_path);
        ClearScreenF(true, true, COLOR_STD_BG);
        return 0;
    }
    else if (user_select == font) { // set font
        u8* font = (u8*) malloc(0x20000); // arbitrary, should be enough by far
        if (!font) return 1;
        u32 font_size = FileGetData(file_path, font, 0x20000, 0);
        if (font_size) SetFont(font, font_size);
        ClearScreenF(true, true, COLOR_STD_BG);
        free(font);
        return 0;
    }
    else if (user_select == translation) { // set translation
        u8* translation = (u8*) malloc(0x20000); // arbitrary, should be enough by far
        if (!translation) return 1;
        u32 translation_size = FileGetData(file_path, translation, 0x20000, 0);
        if (translation_size) SetLanguage(translation, translation_size);
        ClearScreenF(true, true, COLOR_STD_BG);
        free(translation);
        return 0;
    }
    else if (user_select == view) { // view gfx
        if (FileGraphicsViewer(file_path) != 0)
            ShowPrompt(false, "%s\n%s", pathstr, STR_ERROR_CANNOT_VIEW_FILE);
        return 0;
    }
    else if (user_select == agbexport) { // export GBA VC save
        if (DumpGbaVcSavegame(file_path) == 0)
            ShowPrompt(false, STR_SAVEGAME_DUMPED_TO_OUT, OUTPUT_PATH);
        else ShowPrompt(false, "%s", STR_SAVEGAME_DUMP_FAILED);
        return 0;
    }
    else if (user_select == agbimport) { // import GBA VC save
        if (clipboard->n_entries != 1) {
            ShowPrompt(false, "%s", STR_GBA_SAVEGAME_MUST_BE_IN_CLIPBOARD);
        } else {
            ShowPrompt(false, "%s",
                (InjectGbaVcSavegame(file_path, clipboard->entry[0].path) == 0) ? STR_SAVEGAME_INJECT_SUCCESS : STR_SAVEGAME_INJECT_FAILED);
            clipboard->n_entries = 0;
        }
        return 0;
    }
    else if (user_select == setup) { // set as default (font)
        if (filetype & FONT_RIFF) {
            if (SetAsSupportFile("font.frf", file_path))
                ShowPrompt(false, "%s\n%s", pathstr, STR_FONT_WILL_BE_ACTIVE_ON_NEXT_BOOT);
        } else if (filetype & FONT_PBM) {
            if (SetAsSupportFile("font.pbm", file_path))
                ShowPrompt(false, "%s\n%s", pathstr, STR_FONT_WILL_BE_ACTIVE_ON_NEXT_BOOT);
        } else if (filetype & TRANSLATION) {
            if (SetAsSupportFile("language.trf", file_path))
                ShowPrompt(false, "%s\n%s", pathstr, STR_LANGUAGE_WILL_BE_ACTIVE_ON_NEXT_BOOT);
        }
        return 0;
    }

    return FileHandlerMenu(current_path, cursor, scroll, pane);
}

u32 HomeMoreMenu(char* current_path) {
    NandPartitionInfo np_info;
    if (GetNandPartitionInfo(&np_info, NP_TYPE_BONUS, NP_SUBTYPE_CTR, 0, NAND_SYSNAND) != 0) np_info.count = 0;

    const char* optionstr[8];
    const char* promptstr = STR_HOME_MORE_MENU_SELECT_ACTION;
    u32 n_opt = 0;
    int sdformat = ++n_opt;
    int bonus = (np_info.count > 0x2000) ? (int) ++n_opt : -1; // 4MB minsize
    int multi = (CheckMultiEmuNand()) ? (int) ++n_opt : -1;
    int bsupport = ++n_opt;
    int hsrestore = ((CheckHealthAndSafetyInject("1:") == 0) || (CheckHealthAndSafetyInject("4:") == 0)) ? (int) ++n_opt : -1;
    int clock = ++n_opt;
    int bright = ++n_opt;
    int calib = ++n_opt;
    int sysinfo = ++n_opt;
    int readme = (FindVTarFileInfo(VRAM0_README_MD, NULL)) ? (int) ++n_opt : -1;

    if (sdformat > 0) optionstr[sdformat - 1] = STR_SD_FORMAT_MENU;
    if (bonus > 0) optionstr[bonus - 1] = STR_BONUS_DRIVE_MENU;
    if (multi > 0) optionstr[multi - 1] = STR_SWITCH_EMUNAND;
    if (bsupport > 0) optionstr[bsupport - 1] = STR_BUILD_SUPPORT_FILES;
    if (hsrestore > 0) optionstr[hsrestore - 1] = STR_RESTORE_H_AND_S;
    if (clock > 0) optionstr[clock - 1] = STR_SET_RTC_DATE_TIME;
    if (bright > 0) optionstr[bright - 1] = STR_CONFGURE_BRIGHTNESS;
    if (calib > 0) optionstr[calib - 1] = STR_CALIBRATE_TOUCHSCREEN;
    if (sysinfo > 0) optionstr[sysinfo - 1] = STR_SYSTEM_INFO;
    if (readme > 0) optionstr[readme - 1] = STR_SHOW_README;

    int user_select = ShowSelectPrompt(n_opt, optionstr, "%s", promptstr);
    if (user_select == sdformat) { // format SD card
        bool sd_state = CheckSDMountState();
        char slabel[DRV_LABEL_LEN] = { '\0' };
        if (clipboard->n_entries && (DriveType(clipboard->entry[0].path) & (DRV_SDCARD|DRV_ALIAS|DRV_EMUNAND|DRV_IMAGE)))
            clipboard->n_entries = 0; // remove SD clipboard entries
        GetFATVolumeLabel("0:", slabel); // get SD volume label
        DeinitExtFS();
        DeinitSDCardFS();
        if ((SdFormatMenu(slabel) == 0) || sd_state) {;
            while (!InitSDCardFS() &&
                ShowPrompt(true, "%s", STR_INITIALIZING_SD_FAILED_RETRY));
        }
        ClearScreenF(true, true, COLOR_STD_BG);
        AutoEmuNandBase(true);
        InitExtFS();
        GetDirContents(current_dir, current_path);
        return 0;
    }
    else if (user_select == bonus) { // setup bonus drive
        if (clipboard->n_entries && (DriveType(clipboard->entry[0].path) & (DRV_BONUS|DRV_IMAGE)))
            clipboard->n_entries = 0; // remove bonus drive clipboard entries
        if (!SetupBonusDrive()) ShowPrompt(false, "%s", STR_SETUP_FAILED);
        ClearScreenF(true, true, COLOR_STD_BG);
        GetDirContents(current_dir, current_path);
        return 0;
    }
    else if (user_select == multi) { // switch EmuNAND offset
        while (ShowPrompt(true, STR_CURRENT_EMUNAND_OFFSET_IS_N_SWITCH_TO_NEXT, GetEmuNandBase())) {
            if (clipboard->n_entries && (DriveType(clipboard->entry[0].path) & DRV_EMUNAND))
                clipboard->n_entries = 0; // remove EmuNAND clipboard entries
            DismountDriveType(DRV_EMUNAND);
            AutoEmuNandBase(false);
            InitExtFS();
        }
        GetDirContents(current_dir, current_path);
        return 0;
    }
    else if (user_select == bsupport) { // build support files
        bool tik_enc_sys = false;
        bool tik_enc_emu = false;
        if (BuildTitleKeyInfo(NULL, false, false) == 0) {
            ShowString(STR_BUILDING_X_SYSNAND, TIKDB_NAME_ENC);
            tik_enc_sys = (BuildTitleKeyInfo("1:/dbs/ticket.db", false, false) == 0);
            ShowString(STR_BUILDING_X_EMUNAND, TIKDB_NAME_ENC);
            tik_enc_emu = (BuildTitleKeyInfo("4:/dbs/ticket.db", false, false) == 0);
            if (!tik_enc_sys || BuildTitleKeyInfo(NULL, false, true) != 0)
                tik_enc_sys = tik_enc_emu = false;
        }
        bool tik_dec_sys = false;
        bool tik_dec_emu = false;
        if (BuildTitleKeyInfo(NULL, true, false) == 0) {
            ShowString(STR_BUILDING_X_SYSNAND, TIKDB_NAME_DEC);
            tik_dec_sys = (BuildTitleKeyInfo("1:/dbs/ticket.db", true, false) == 0);
            ShowString(STR_BUILDING_X_EMUNAND, TIKDB_NAME_DEC);
            tik_dec_emu = (BuildTitleKeyInfo("4:/dbs/ticket.db", true, false) == 0);
            if (!tik_dec_sys || BuildTitleKeyInfo(NULL, true, true) != 0)
                tik_dec_sys = tik_dec_emu = false;
        }
        bool seed_sys = false;
        bool seed_emu = false;
        if (BuildSeedInfo(NULL, false) == 0) {
            ShowString(STR_BUILDING_X_SYSNAND, SEEDINFO_NAME);
            seed_sys = (BuildSeedInfo("1:", false) == 0);
            ShowString(STR_BUILDING_X_EMUNAND, SEEDINFO_NAME);
            seed_emu = (BuildSeedInfo("4:", false) == 0);
            if (!seed_sys || BuildSeedInfo(NULL, true) != 0)
                seed_sys = seed_emu = false;
        }
        ShowPrompt(false, STR_BUILT_IN_OUT_STATUSES, OUTPUT_PATH,
            TIKDB_NAME_ENC, tik_enc_sys ? tik_enc_emu ? STR_OK_SYS_EMU : STR_OK_SYS : STR_FAILED,
            TIKDB_NAME_DEC, tik_dec_sys ? tik_dec_emu ? STR_OK_SYS_EMU : STR_OK_SYS : STR_FAILED,
            SEEDINFO_NAME, seed_sys ? seed_emu ? STR_OK_SYS_EMU : STR_OK_SYS : STR_FAILED);
        GetDirContents(current_dir, current_path);
        return 0;
    }
    else if (user_select == hsrestore) { // restore Health & Safety
        n_opt = 0;
        int sys = (CheckHealthAndSafetyInject("1:") == 0) ? (int) ++n_opt : -1;
        int emu = (CheckHealthAndSafetyInject("4:") == 0) ? (int) ++n_opt : -1;
        if (sys > 0) optionstr[sys - 1] = STR_RESTORE_H_AND_S_EMUNAND;
        if (emu > 0) optionstr[emu - 1] = STR_RESTORE_H_AND_S_SYSNAND;
        user_select = (n_opt > 1) ? ShowSelectPrompt(n_opt, optionstr, "%s", promptstr) : n_opt;
        if (user_select > 0) {
            InjectHealthAndSafety(NULL, (user_select == sys) ? "1:" : "4:");
            GetDirContents(current_dir, current_path);
            return 0;
        }
    }
    else if (user_select == clock) { // RTC clock setter
        DsTime dstime;
        get_dstime(&dstime);
        if (ShowRtcSetterPrompt(&dstime, "%s", STR_TITLE_SET_RTC_DATE_TIME)) {
            char timestr[UTF_BUFFER_BYTESIZE(32)];
            set_dstime(&dstime);
            GetTimeString(timestr, true, true);
            ShowPrompt(false, STR_NEW_RTC_DATE_TIME_IS_TIME, timestr);
        }
        return 0;
    }
    else if (user_select == bright) { // brightness config dialogue
        s32 old_brightness, new_brightness;
        if (!LoadSupportFile("gm9bright.cfg", &old_brightness, 4))
            old_brightness = BRIGHTNESS_AUTOMATIC; // auto by default
        new_brightness = ShowBrightnessConfig(old_brightness);
        if (old_brightness != new_brightness)
            SaveSupportFile("gm9bright.cfg", &new_brightness, 4);
        return 0;
    }
    else if (user_select == calib) { // touchscreen calibration
        ShowPrompt(false, "%s",
            (ShowTouchCalibrationDialog()) ? STR_TOUCHSCREEN_CALIBRATION_SUCCESS : STR_TOUCHSCREEN_CALIBRATION_FAILED);
        return 0;
    }
    else if (user_select == sysinfo) { // Myria's system info
        char* sysinfo_txt = (char*) malloc(STD_BUFFER_SIZE);
        if (!sysinfo_txt) return 1;
        MyriaSysinfo(sysinfo_txt);
        MemTextViewer(sysinfo_txt, strnlen(sysinfo_txt, STD_BUFFER_SIZE), 1, false);
        free(sysinfo_txt);
        return 0;
    }
    else if (user_select == readme) { // Display GodMode9 readme
        u64 README_md_size;
        char* README_md = FindVTarFileInfo(VRAM0_README_MD, &README_md_size);
        MemToCViewer(README_md, README_md_size, STR_GODMODE9_README_TOC);
        return 0;
    } else return 1;

    return HomeMoreMenu(current_path);
}

u32 GodMode(int entrypoint) {
    const u32 quick_stp = (MAIN_SCREEN == TOP_SCREEN) ? 20 : 19;
    u32 exit_mode = GODMODE_EXIT_POWEROFF;

    char current_path[256] = { 0x00 }; // don't change this size!
    u32 cursor = 0;
    u32 scroll = 0;

    int mark_next = -1;
    u32 last_write_perm = GetWritePermissions();
    u32 last_clipboard_size = 0;

    bool bootloader = IS_UNLOCKED && (entrypoint == ENTRY_NANDBOOT);
    bool bootmenu = bootloader && (BOOTMENU_KEY != BUTTON_START) && CheckButton(BOOTMENU_KEY);
    bool godmode9 = !bootloader;


    // FIRM from FCRAM handling
    FirmHeader* firm_in_mem = (FirmHeader*) __FIRMTMP_ADDR; // should be safe here
    if (bootloader) { // check for FIRM in FCRAM, but prevent bootloops
        void* addr = (void*) __FIRMRAM_ADDR;
        u32 firm_size = GetFirmSize((FirmHeader*) addr);
        memcpy(firm_in_mem, "NOPE", 4); // overwrite header to prevent bootloops
        if (firm_size && (firm_size <= (__FIRMRAM_END - __FIRMRAM_ADDR))) {
            memcpy(firm_in_mem, addr, firm_size);
            memcpy(addr, "NOPE", 4); // to prevent bootloops
        }
    }

    // get mode string for splash screen
    const char* disp_mode = NULL;
    if (bootloader) disp_mode = "bootloader mode\nR+LEFT for menu";
    else if (!IS_UNLOCKED && (entrypoint == ENTRY_NANDBOOT)) disp_mode = "oldloader mode";
    else if (entrypoint == ENTRY_NTRBOOT) disp_mode = "ntrboot mode";
    else if (entrypoint == ENTRY_UNKNOWN) disp_mode = "unknown mode";

    bool show_splash = true;
    #ifdef SALTMODE
    show_splash = !bootloader;
    #endif

    // init font
    if (!SetFont(NULL, 0)) return exit_mode;

    // show splash screen (if enabled)
    ClearScreenF(true, true, COLOR_STD_BG);
    if (show_splash) SplashInit(disp_mode);
    u64 timer = timer_start(); // for splash delay

    InitSDCardFS();
    AutoEmuNandBase(true);
    InitNandCrypto(true); // (entrypoint != ENTRY_B9S);
    InitExtFS();
    if (!CalibrateTouchFromSupportFile())
        CalibrateTouchFromFlash();

    // brightness from file?
    s32 brightness = -1;
    if (LoadSupportFile("gm9bright.cfg", &brightness, 0x4))
        SetScreenBrightness(brightness);

    // custom font handling
    if (CheckSupportFile("font.frf")) {
        u8* riff = (u8*) malloc(0x20000); // arbitrary, should be enough by far
        if (riff) {
            u32 riff_size = LoadSupportFile("font.frf", riff, 0x20000);
            if (riff_size) SetFont(riff, riff_size);
            free(riff);
        }
    } else if (CheckSupportFile("font.pbm")) {
        u8* pbm = (u8*) malloc(0x10000); // arbitrary, should be enough by far
        if (pbm) {
            u32 pbm_size = LoadSupportFile("font.pbm", pbm, 0x10000);
            if (pbm_size) SetFont(pbm, pbm_size);
            free(pbm);
        }
    }

    // language handling
    bool language_loaded = false;
    if (CheckSupportFile("language.trf")) {
        char* translation = (char*) malloc(0x20000); // arbitrary, should be enough by far
        if (translation) {
            u32 translation_size = LoadSupportFile("language.trf", translation, 0x20000);
            if (translation_size) language_loaded = SetLanguage(translation, translation_size);
            free(translation);
        }
    }

    if (!language_loaded) {
        SetLanguage(NULL, 0);

        char loadpath[256];
        if (LanguageMenu(loadpath, "Select Language for GodMode9:")) {
            size_t fsize = FileGetSize(loadpath);
            if (fsize > 0) {
                char* data = (char*)malloc(fsize);
                if (data) {
                    FileGetData(loadpath, data, fsize, 0);
                    SaveSupportFile("language.trf", data, fsize);
                    SetLanguage(data, fsize);
                    free(data);
                }
            }

            // Try load font with the same name
            char *ext = strstr(loadpath, ".trf");
            strcpy(ext, ".frf");
            fsize = FileGetSize(loadpath);
            if (fsize > 0) {
                char* data = (char*)malloc(fsize);
                if (data) {
                    FileGetData(loadpath, data, fsize, 0);
                    SaveSupportFile("font.frf", data, fsize);
                    SetFont(data, fsize);
                    free(data);
                }
            }
        }
    }

    // check for embedded essential backup
    if (((entrypoint == ENTRY_NANDBOOT) || (entrypoint == ENTRY_B9S)) &&
        !PathExist("S:/essential.exefs") && CheckGenuineNandNcsd() &&
        ShowPrompt(true, "%s", STR_ESSENTIAL_BACKUP_NOT_FOUND_CREATE_NOW)) {
        if (EmbedEssentialBackup("S:/nand.bin") == 0) {
            u32 flags = BUILD_PATH | SKIP_ALL;
            PathCopy(OUTPUT_PATH, "S:/essential.exefs", &flags);
            ShowPrompt(false, STR_BACKUP_EMBEDDED_WRITTEN_TO_OUT, OUTPUT_PATH);
        }
    }

    // note: this is kinda duplicated in arm9/source/lua/gm9internalsys.c as well
    // check internal clock
    if (IS_UNLOCKED) { // we could actually do this on any entrypoint
        DsTime dstime;
        get_dstime(&dstime);
        if ((DSTIMEGET(&dstime, bcd_Y) < 18) &&
             ShowPrompt(true, "%s", STR_RTC_DATE_TIME_SEEMS_TO_BE_WRONG_SET_NOW) &&
             ShowRtcSetterPrompt(&dstime, "%s", STR_TITLE_SET_RTC_DATE_TIME)) {
            char timestr[UTF_BUFFER_BYTESIZE(32)];
            set_dstime(&dstime);
            GetTimeString(timestr, true, true);
            ShowPrompt(false, STR_NEW_RTC_DATE_TIME_IS_TIME, timestr);
        }
    }

    #if defined(SALTMODE)
    show_splash = bootmenu = (bootloader && CheckButton(BOOTMENU_KEY));
    if (show_splash) SplashInit("saltmode");
    #else // standard behaviour
    bootmenu = bootmenu || (bootloader && CheckButton(BOOTMENU_KEY)); // second check for boot menu keys
    #endif
    while (CheckButton(BOOTPAUSE_KEY)); // don't continue while these keys are held
    if (show_splash) while (timer_msec( timer ) < 1000); // show splash for at least 1 sec

    // bootmenu handler
    if (bootmenu) {
        bootloader = false;
        while (HID_ReadState() & BUTTON_ANY); // wait until no buttons are pressed
        while (!bootloader && !godmode9) {
            const char* optionstr[6] = { STR_RESUME_GODMODE9, STR_RESUME_BOOTLOADER, STR_SELECT_PAYLOAD, STR_SELECT_SCRIPT,
                STR_POWEROFF_SYSTEM, STR_REBOOT_SYSTEM };
            int user_select = ShowSelectPrompt(6, optionstr, STR_FLAVOR_BOOTLOADER_SELECT_OPTION, FLAVOR);
            char loadpath[256];
            if (user_select == 1) {
                godmode9 = true;
            } else if (user_select == 2) {
                bootloader = true;
            } else if ((user_select == 3) && (FileSelectorSupport(loadpath, STR_BOOTLOADER_PAYLOADS_MENU_SELECT_PAYLOAD, PAYLOADS_DIR, "*.firm"))) {
                BootFirmHandler(loadpath, false, false);
            } else if ((user_select == 4) && (FileSelectorSupport(loadpath, STR_BOOTLOADER_SCRIPTS_MENU_SELECT_SCRIPT, SCRIPTS_DIR, "*.gm9"))) {
                ExecuteGM9Script(loadpath);
            } else if (user_select == 5) {
                exit_mode = GODMODE_EXIT_POWEROFF;
            } else if (user_select == 6) {
                exit_mode = GODMODE_EXIT_REBOOT;
            } else if (user_select) continue;
            break;
        }
    }

    // bootloader handler
    if (bootloader) {
        const char* bootfirm_paths[] = { BOOTFIRM_PATHS };
        if (IsBootableFirm(firm_in_mem, FIRM_MAX_SIZE)) {
            DeinitExtFS();
            DeinitSDCardFS();
            PXI_DoCMD(PXICMD_LEGACY_BOOT, NULL, 0);
            PXI_Barrier(PXI_FIRMLAUNCH_BARRIER);
            BootFirm(firm_in_mem, "sdmc:/bootonce.firm");
        }
        for (u32 i = 0; i < sizeof(bootfirm_paths) / sizeof(char*); i++) {
            BootFirmHandler(bootfirm_paths[i], false, (BOOTFIRM_TEMPS >> i) & 0x1);
        }
        ShowPrompt(false, "%s", STR_NO_BOOTABLE_FIRM_FOUND_RESUMING_GODMODE9);
        godmode9 = true;
    }

    if (godmode9) {
        current_dir = (DirStruct*) malloc(sizeof(DirStruct));
        clipboard = (DirStruct*) malloc(sizeof(DirStruct));
        panedata = (PaneData*) malloc(N_PANES * sizeof(PaneData));
        if (!current_dir || !clipboard || !panedata) {
            ShowPrompt(false, "%s", STR_OUT_OF_MEMORY); // just to be safe
            return exit_mode;
        }

        GetDirContents(current_dir, "");
        clipboard->n_entries = 0;
        memset(panedata, 0x00, N_PANES * sizeof(PaneData));
        ClearScreenF(true, true, COLOR_STD_BG); // clear splash
    }

    PaneData* pane = panedata;
    while (godmode9) { // this is the main loop
        // basic sanity checking
        if (!current_dir->n_entries) { // current dir is empty -> revert to root
            ShowPrompt(false, "%s", STR_INVALID_DIRECTORY_OBJECT);
            *current_path = '\0';
            SetTitleManagerMode(false);
            DeinitExtFS(); // deinit and...
            InitExtFS(); // reinitialize extended file system
            GetDirContents(current_dir, current_path);
            cursor = 0;
            if (!current_dir->n_entries) { // should not happen, if it does fail gracefully
                ShowPrompt(false, "%s", STR_INVALID_ROOT_DIRECTORY);
                return exit_mode;
            }
        }
        if (cursor >= current_dir->n_entries) // cursor beyond allowed range
            cursor = current_dir->n_entries - 1;

        int curr_drvtype = DriveType(current_path);
        DirEntry* curr_entry = &(current_dir->entry[cursor]);
        if ((mark_next >= 0) && (curr_entry->type != T_DOTDOT)) {
            curr_entry->marked = mark_next;
            mark_next = -2;
        }
        DrawDirContents(current_dir, cursor, &scroll);
        DrawUserInterface(current_path, curr_entry, N_PANES ? pane - panedata + 1 : 0);
        DrawTopBar(current_path);

        // check write permissions
        if (~last_write_perm & GetWritePermissions()) {
            if (ShowPrompt(true, "%s", STR_WRITE_PERMISSIONS_WERE_CHANGED_RELOCK)) SetWritePermissions(last_write_perm, false);
            last_write_perm = GetWritePermissions();
            continue;
        }

        // handle user input
        u32 pad_state = InputWait(3);
        bool switched = (pad_state & BUTTON_R1);

        // basic navigation commands
        if ((pad_state & BUTTON_A) && (curr_entry->type != T_FILE) && (curr_entry->type != T_DOTDOT)) { // for dirs
            if (switched && !(DriveType(curr_entry->path) & (DRV_SEARCH|DRV_TITLEMAN))) { // exclude Y/Z
                const char* optionstr[8] = { NULL };
                char tpath[16], copyToOut[UTF_BUFFER_BYTESIZE(64)], dumpToOut[UTF_BUFFER_BYTESIZE(64)];
                snprintf(tpath, sizeof(tpath), "%2.2s/dbs/title.db", curr_entry->path);
                snprintf(copyToOut, sizeof(copyToOut), STR_COPY_TO_OUT, OUTPUT_PATH);
                snprintf(dumpToOut, sizeof(dumpToOut), STR_DUMP_TO_OUT, OUTPUT_PATH);
                int n_opt = 0;
                int tman = (!(DriveType(curr_entry->path) & DRV_IMAGE) &&
                    ((strncmp(curr_entry->path, tpath, 16) == 0) ||
                     (!*current_path && PathExist(tpath)))) ? ++n_opt : -1;
                int srch_f = ++n_opt;
                int fixcmac = (!*current_path && ((strspn(curr_entry->path, "14AB") == 1) ||
                    ((GetMountState() == IMG_NAND) && (*(curr_entry->path) == '7')))) ? ++n_opt : -1;
                int dirnfo = ++n_opt;
                int stdcpy = (*current_path && strncmp(current_path, OUTPUT_PATH, 256) != 0) ? ++n_opt : -1;
                int rawdump = (!*current_path && (DriveType(curr_entry->path) & DRV_CART)) ? ++n_opt : -1;
                if (tman > 0) optionstr[tman-1] = STR_OPEN_TITLE_MANAGER;
                if (srch_f > 0) optionstr[srch_f-1] = STR_SEARCH_FOR_FILES;
                if (fixcmac > 0) optionstr[fixcmac-1] = STR_FIX_CMACS_FOR_DRIVE;
                if (dirnfo > 0) optionstr[dirnfo-1] = (*current_path) ? STR_SHOW_DIRECTORY_INFO : STR_SHOW_DRIVE_INFO;
                if (stdcpy > 0) optionstr[stdcpy-1] = copyToOut;
                if (rawdump > 0) optionstr[rawdump-1] = dumpToOut;
                char namestr[UTF_BUFFER_BYTESIZE(32)];
                TruncateString(namestr, (*current_path) ? curr_entry->path : curr_entry->name, 32, 8);
                int user_select = ShowSelectPrompt(n_opt, optionstr, "%s", namestr);
                if (user_select == tman) {
                    if (InitImgFS(tpath)) {
                        SetTitleManagerMode(true);
                        snprintf(current_path, sizeof(current_path), "Y:");
                        GetDirContents(current_dir, current_path);
                        cursor = 1;
                        scroll = 0;
                    } else ShowPrompt(false, "%s", STR_FAILED_SETTING_UP_TITLE_MANAGER);
                } else if (user_select == srch_f) {
                    char searchstr[256];
                    snprintf(searchstr, sizeof(searchstr), "*");
                    TruncateString(namestr, curr_entry->name, 20, 8);
                    if (ShowKeyboardOrPrompt(searchstr, 256, STR_SEARCH_FILE_ENTER_SEARCH_BELOW, namestr)) {
                        SetFSSearch(searchstr, curr_entry->path);
                        snprintf(current_path, sizeof(current_path), "Z:");
                        GetDirContents(current_dir, current_path);
                        if (current_dir->n_entries) ShowPrompt(false, STR_FOUND_N_RESULTS, current_dir->n_entries - 1);
                        cursor = 1;
                        scroll = 0;
                    }
                } else if (user_select == fixcmac) {
                    RecursiveFixFileCmac(curr_entry->path);
                    ShowPrompt(false, "%s", STR_FIX_CMACS_FOR_DRIVE_FINISHED);
                } else if (user_select == dirnfo) {
                    if (DirFileAttrMenu(curr_entry->path, curr_entry->name)) {
                        ShowPrompt(false, "%s",(current_path[0] == '\0') ? STR_FAILED_TO_ANALYZE_DRIVE : STR_FAILED_TO_ANALYZE_DIR);
                    }
                } else if (user_select == stdcpy) {
                    StandardCopy(&cursor, &scroll);
                } else if (user_select == rawdump) {
                    CartRawDump();
                }
            } else { // one level up
                u32 user_select = 1;
                if (curr_drvtype & DRV_SEARCH) { // special menu for search drive
                    const char* optionstr[2] = { STR_OPEN_THIS_FOLDER, STR_OPEN_CONTAINING_FOLDER };
                    char pathstr[UTF_BUFFER_BYTESIZE(32)];
                    TruncateString(pathstr, curr_entry->path, 32, 8);
                    user_select = ShowSelectPrompt(2, optionstr, "%s", pathstr);
                }
                if (user_select) {
                    strncpy(current_path, curr_entry->path, 256);
                    current_path[255] = '\0';
                    if (user_select == 2) {
                        char* last_slash = strrchr(current_path, '/');
                        if (last_slash) *last_slash = '\0';
                    }
                    GetDirContents(current_dir, current_path);
                    if (*current_path && (current_dir->n_entries > 1)) {
                        cursor = 1;
                        scroll = 0;
                    } else cursor = 0;
                }
            }
        } else if ((pad_state & BUTTON_A) && (curr_entry->type == T_FILE)) { // process a file
            if (!curr_entry->marked) ShowGameFileIcon(curr_entry->path, ALT_SCREEN);
            DrawTopBar(current_path);
            FileHandlerMenu(current_path, &cursor, &scroll, &pane); // processed externally
            ClearScreenF(true, true, COLOR_STD_BG);
        } else if (*current_path && ((pad_state & BUTTON_B) || // one level down
            ((pad_state & BUTTON_A) && (curr_entry->type == T_DOTDOT)))) {
            if (switched) { // use R+B to return to root fast
                *current_path = '\0';
                GetDirContents(current_dir, current_path);
                cursor = scroll = 0;
            } else {
                char old_path[256];
                char* last_slash = strrchr(current_path, '/');
                strncpy(old_path, current_path, 256);
                if (last_slash) *last_slash = '\0';
                else *current_path = '\0';
                GetDirContents(current_dir, current_path);
                if (*old_path && current_dir->n_entries) {
                    for (cursor = current_dir->n_entries - 1;
                        (cursor > 0) && (strncmp(current_dir->entry[cursor].path, old_path, 256) != 0); cursor--);
                    if (*current_path && !cursor && (current_dir->n_entries > 1)) cursor = 1; // don't set it on the dotdot
                    scroll = 0;
                }
            }
        } else if (switched && (pad_state & BUTTON_B)) { // unmount SD card
            if (!CheckSDMountState()) {
                while (!InitSDCardFS() &&
                    ShowPrompt(true, "%s", STR_INITIALIZING_SD_FAILED_RETRY));
            } else {
                DeinitSDCardFS();
                if (clipboard->n_entries && !PathExist(clipboard->entry[0].path))
                    clipboard->n_entries = 0; // remove SD clipboard entries
            }
            ClearScreenF(true, true, COLOR_STD_BG);
            AutoEmuNandBase(true);
            InitExtFS();
            GetDirContents(current_dir, current_path);
            if (cursor >= current_dir->n_entries) cursor = 0;
        } else if (!switched && (pad_state & BUTTON_DOWN) && (cursor + 1 < current_dir->n_entries))  { // cursor down
            if (pad_state & BUTTON_L1) mark_next = curr_entry->marked;
            cursor++;
        } else if (!switched && (pad_state & BUTTON_UP) && cursor) { // cursor up
            if (pad_state & BUTTON_L1) mark_next = curr_entry->marked;
            cursor--;
        } else if (switched && (pad_state & (BUTTON_RIGHT|BUTTON_LEFT))) { // switch pane
            memcpy(pane->path, current_path, 256);  // store state in current pane
            pane->cursor = cursor;
            pane->scroll = scroll;
            (pad_state & BUTTON_LEFT) ? pane-- : pane++; // switch to next
            if (pane < panedata) pane += N_PANES;
            else if (pane >= panedata + N_PANES) pane -= N_PANES;
            memcpy(current_path, pane->path, 256);  // get state from next pane
            cursor = pane->cursor;
            scroll = pane->scroll;
            GetDirContents(current_dir, current_path);
        } else if (switched && (pad_state & BUTTON_DOWN)) { // force reload file list
            GetDirContents(current_dir, current_path);
            ClearScreenF(true, true, COLOR_STD_BG);
        } else if ((pad_state & BUTTON_RIGHT) && !(pad_state & BUTTON_L1)) { // cursor down (quick)
            cursor += quick_stp;
        } else if ((pad_state & BUTTON_LEFT) && !(pad_state & BUTTON_L1)) { // cursor up (quick)
            cursor = (cursor >= quick_stp) ? cursor - quick_stp : 0;
        } else if ((pad_state & BUTTON_RIGHT) && *current_path) { // mark all entries
            for (u32 c = 1; c < current_dir->n_entries; c++) current_dir->entry[c].marked = 1;
            mark_next = 1;
        } else if ((pad_state & BUTTON_LEFT) && *current_path) { // unmark all entries
            for (u32 c = 1; c < current_dir->n_entries; c++) current_dir->entry[c].marked = 0;
            mark_next = 0;
        } else if (switched && (pad_state & BUTTON_L1)) { // switched L -> screenshot
            // this is handled in hid.h
        } else if (*current_path && (pad_state & BUTTON_L1) && (curr_entry->type != T_DOTDOT)) {
            // unswitched L - mark/unmark single entry
            if (mark_next < -1) mark_next = -1;
            else curr_entry->marked ^= 0x1;
        } else if (pad_state & BUTTON_SELECT) { // clear/restore clipboard
            clipboard->n_entries = (clipboard->n_entries > 0) ? 0 : last_clipboard_size;
        }

        // highly specific commands
        if (!*current_path) { // in the root folder...
            if (switched && (pad_state & BUTTON_X)) { // unmount image
                if (clipboard->n_entries && (DriveType(clipboard->entry[0].path) & DRV_IMAGE))
                    clipboard->n_entries = 0; // remove last mounted image clipboard entries
                SetTitleManagerMode(false);
                InitImgFS(NULL);
                ClearScreenF(false, true, COLOR_STD_BG);
                GetDirContents(current_dir, current_path);
            } else if (switched && (pad_state & BUTTON_Y)) {
                SetWritePermissions(PERM_BASE, false);
                last_write_perm = GetWritePermissions();
                ClearScreenF(false, true, COLOR_STD_BG);
            }
        } else if (!switched) { // standard unswitched command set
            if ((curr_drvtype & DRV_VIRTUAL) && (pad_state & BUTTON_X) && (*current_path != 'T')) {
                ShowPrompt(false, "%s", STR_NOT_ALLOWED_IN_VIRTUAL_PATH);
            } else if (pad_state & BUTTON_X) { // delete a file
                u32 n_marked = 0;
                if (curr_entry->marked) {
                    for (u32 c = 0; c < current_dir->n_entries; c++)
                        if (current_dir->entry[c].marked) n_marked++;
                }
                if (n_marked) {
                    if (ShowPrompt(true, STR_DELETE_N_PATHS, n_marked)) {
                        u32 n_errors = 0;
                        ShowString("%s", STR_DELETING_FILES_PLEASE_WAIT);
                        for (u32 c = 0; c < current_dir->n_entries; c++)
                            if (current_dir->entry[c].marked && !PathDelete(current_dir->entry[c].path))
                                n_errors++;
                        ClearScreenF(true, false, COLOR_STD_BG);
                        if (n_errors) ShowPrompt(false, STR_FAILED_DELETING_N_OF_N_PATHS, n_errors, n_marked);
                    }
                } else if (curr_entry->type != T_DOTDOT) {
                    char namestr[UTF_BUFFER_BYTESIZE(28)];
                    TruncateString(namestr, curr_entry->name, 28, 12);
                    if (ShowPrompt(true, STR_DELETE_FILE, namestr)) {
                        ShowString("%s", STR_DELETING_FILES_PLEASE_WAIT);
                        if (!PathDelete(curr_entry->path))
                            ShowPrompt(false, STR_FAILED_DELETING_PATH, namestr);
                        ClearScreenF(true, false, COLOR_STD_BG);
                    }
                }
                GetDirContents(current_dir, current_path);
            } else if ((pad_state & BUTTON_Y) && (clipboard->n_entries == 0)) { // fill clipboard
                for (u32 c = 0; c < current_dir->n_entries; c++) {
                    if (current_dir->entry[c].marked) {
                        current_dir->entry[c].marked = 0;
                        DirEntryCpy(&(clipboard->entry[clipboard->n_entries]), &(current_dir->entry[c]));
                        clipboard->n_entries++;
                    }
                }
                if ((clipboard->n_entries == 0) && (curr_entry->type != T_DOTDOT)) {
                    DirEntryCpy(&(clipboard->entry[0]), curr_entry);
                    clipboard->n_entries = 1;
                }
                if (clipboard->n_entries)
                    last_clipboard_size = clipboard->n_entries;
            } else if ((curr_drvtype & DRV_SEARCH) && (pad_state & BUTTON_Y)) {
                ShowPrompt(false, "%s", STR_NOT_ALLOWED_IN_SEARCH_DRIVE);
            } else if ((curr_drvtype & DRV_GAME) && (pad_state & BUTTON_Y)) {
                ShowPrompt(false, "%s", STR_NOT_ALLOWED_IN_VIRTUAL_GAME_PATH);
            } else if ((curr_drvtype & DRV_XORPAD) && (pad_state & BUTTON_Y)) {
                ShowPrompt(false, "%s", STR_NOT_ALLOWED_IN_XORPAD_DRIVE);
            } else if ((curr_drvtype & DRV_CART) && (pad_state & BUTTON_Y)) {
                ShowPrompt(false, "%s", STR_NOT_ALLOWED_IN_GAMECART_DRIVE);
            } else if (pad_state & BUTTON_Y) { // paste files
                const char* optionstr[2] = { STR_COPY_PATHS, STR_MOVE_PATHS };
                char promptstr[UTF_BUFFER_BYTESIZE(64)];
                u32 flags = 0;
                u32 user_select;
                if (clipboard->n_entries == 1) {
                    char namestr[UTF_BUFFER_BYTESIZE(20)];
                    TruncateString(namestr, clipboard->entry[0].name, 20, 12);
                    snprintf(promptstr, sizeof(promptstr), STR_PASTE_FILE_HERE, namestr);
                } else snprintf(promptstr, sizeof(promptstr), STR_PASTE_N_PATHS_HERE, clipboard->n_entries);
                user_select = ((DriveType(clipboard->entry[0].path) & curr_drvtype & DRV_STDFAT)) ?
                    ShowSelectPrompt(2, optionstr, "%s", promptstr) : (ShowPrompt(true, "%s", promptstr) ? 1 : 0);
                if (user_select) {
                    for (u32 c = 0; c < clipboard->n_entries; c++) {
                        char namestr[UTF_BUFFER_BYTESIZE(36)];
                        TruncateString(namestr, clipboard->entry[c].name, 36, 12);
                        flags &= ~ASK_ALL;
                        if (c < clipboard->n_entries - 1) flags |= ASK_ALL;
                        if ((user_select == 1) && !PathCopy(current_path, clipboard->entry[c].path, &flags)) {
                            if (c + 1 < clipboard->n_entries) {
                                if (!ShowPrompt(true, STR_FAILED_COPYING_PATH_PROCESS_REMAINING, namestr)) break;
                            } else ShowPrompt(false, STR_FAILED_COPYING_PATH, namestr);
                        } else if ((user_select == 2) && !PathMove(current_path, clipboard->entry[c].path, &flags)) {
                            if (c + 1 < clipboard->n_entries) {
                                if (!ShowPrompt(true, STR_FAILED_MOVING_PATH_PROCESS_REMAINING, namestr)) break;
                            } else ShowPrompt(false, STR_FAILED_MOVING_PATH, namestr);
                        }
                    }
                    clipboard->n_entries = 0;
                    GetDirContents(current_dir, current_path);
                }
                ClearScreenF(true, false, COLOR_STD_BG);
            }
        } else { // switched command set
            if ((curr_drvtype & DRV_VIRTUAL) && (pad_state & (BUTTON_X|BUTTON_Y))) {
                ShowPrompt(false, "%s", STR_NOT_ALLOWED_IN_VIRTUAL_PATH);
            } else if ((curr_drvtype & DRV_ALIAS) && (pad_state & (BUTTON_X))) {
                ShowPrompt(false, "%s", STR_NOT_ALLOWED_IN_ALIAS_PATH);
            } else if ((pad_state & BUTTON_X) && (curr_entry->type != T_DOTDOT)) { // rename a file
                char newname[256];
                char namestr[UTF_BUFFER_BYTESIZE(20)];
                TruncateString(namestr, curr_entry->name, 20, 12);
                snprintf(newname, sizeof(newname), "%s", curr_entry->name);
                if (ShowKeyboardOrPrompt(newname, 256, STR_RENAME_FILE_ENTER_NEW_NAME_BELOW, namestr)) {
                    if (!PathRename(curr_entry->path, newname))
                        ShowPrompt(false, STR_FAILED_RENAMING_PATH, namestr);
                    else {
                        GetDirContents(current_dir, current_path);
                        for (cursor = (current_dir->n_entries) ? current_dir->n_entries - 1 : 0;
                            (cursor > 1) && (strncmp(current_dir->entry[cursor].name, newname, 256) != 0); cursor--);
                    }
                }
            } else if (pad_state & BUTTON_Y) { // create an entry
                const char* optionstr[] = { STR_CREATE_A_FOLDER, STR_CREATE_A_DUMMY_FILE };
                u32 type = ShowSelectPrompt(2, optionstr, "%s", STR_CREATE_A_NEW_ENTRY_HERE_SELECT_TYPE);
                if (type) {
                    char ename[256];
                    u64 fsize = 0;
                    snprintf(ename, sizeof(ename), (type == 1) ? "newdir" : "dummy.bin");
                    if ((ShowKeyboardOrPrompt(ename, 256, "%s", (type == 1) ? STR_CREATE_NEW_FOLDER_HERE_ENTER_NAME_BELOW : STR_CREATE_NEW_FILE_HERE_ENTER_NAME_BELOW)) &&
                        ((type != 2) || ((fsize = ShowNumberPrompt(0, "%s", STR_CREATE_NEW_FILE_HERE_ENTER_SIZE_BELOW)) != (u64) -1))) {
                        if (((type == 1) && !DirCreate(current_path, ename)) ||
                            ((type == 2) && !FileCreateDummy(current_path, ename, fsize))) {
                            char namestr[UTF_BUFFER_BYTESIZE(36)];
                            TruncateString(namestr, ename, 36, 12);
                            ShowPrompt(false, (type == 1) ? STR_FAILED_CREATING_FOLDER_PATH : STR_FAILED_CREATING_FILE_PATH, namestr);
                        } else {
                            GetDirContents(current_dir, current_path);
                            for (cursor = (current_dir->n_entries) ? current_dir->n_entries - 1 : 0;
                                (cursor > 1) && (strncmp(current_dir->entry[cursor].name, ename, 256) != 0); cursor--);
                        }
                    }
                }
            }
        }

        if (pad_state & BUTTON_START) {
            exit_mode = (switched || (pad_state & BUTTON_LEFT)) ? GODMODE_EXIT_POWEROFF : GODMODE_EXIT_REBOOT;
            break;
        } else if (pad_state & (BUTTON_HOME|BUTTON_POWER)) { // Home menu
            const char* optionstr[9];
            bool buttonhome = (pad_state & BUTTON_HOME);
            u32 n_opt = 0;
            int poweroff = ++n_opt;
            int reboot = ++n_opt;
            int language = ++n_opt;
            int brick = (HID_ReadState() & BUTTON_R1) ? ++n_opt : 0;
            int titleman = ++n_opt;
            int scripts = ++n_opt;
#ifndef NO_LUA
            int luascripts = ++n_opt;
#endif
            int payloads = ++n_opt;
            int more = ++n_opt;
            if (poweroff > 0) optionstr[poweroff - 1] = STR_POWEROFF_SYSTEM;
            if (reboot > 0) optionstr[reboot - 1] = STR_REBOOT_SYSTEM;
            if (titleman > 0) optionstr[titleman - 1] = STR_TITLE_MANAGER;
            if (language > 0) optionstr[language - 1] = STR_LANGUAGE;
            if (brick > 0) optionstr[brick - 1] = STR_BRICK_MY_3DS;
            if (scripts > 0) optionstr[scripts - 1] = STR_SCRIPTS;
#ifndef NO_LUA
            if (luascripts > 0) optionstr[luascripts - 1] = STR_LUA_SCRIPTS;
#endif
            if (payloads > 0) optionstr[payloads - 1] = STR_PAYLOADS;
            if (more > 0) optionstr[more - 1] = STR_MORE;

            int user_select = 0;
            while ((user_select = ShowSelectPrompt(n_opt, optionstr, "%s", buttonhome ? STR_HOME_BUTTON_PRESSED_SELECT_ACTION : STR_POWER_BUTTON_PRESSED_SELECT_ACTION)) &&
                (user_select != poweroff) && (user_select != reboot)) {
                char loadpath[256];
                if ((user_select == more) && (HomeMoreMenu(current_path) == 0)) break; // more... menu
                else if (user_select == titleman) {
                    const char* tmoptionstr[4] = {
                        STR_A_DRIVE_SD_CARD,
                        STR_1_DRIVE_NAND_TWL,
                        STR_B_DRIVE_SD_CARD,
                        STR_4_DRIVE_NAND_TWL
                    };
                    static const char* tmpaths[4] = {
                        "A:/dbs/title.db",
                        "1:/dbs/title.db",
                        "B:/dbs/title.db",
                        "4:/dbs/title.db"
                    };
                    u32 tmnum = 2;
                    if (!CheckSDMountState() || (tmnum = ShowSelectPrompt(
                        (CheckVirtualDrive("E:")) ? 4 : 2, tmoptionstr, "%s",
                        STR_TITLE_MANAGER_MENU_SELECT_TITLES_SOURCE))) {
                        const char* tpath = tmpaths[tmnum-1];
                        if (InitImgFS(tpath)) {
                            SetTitleManagerMode(true);
                            snprintf(current_path, sizeof(current_path), "Y:");
                            GetDirContents(current_dir, current_path);
                            ClearScreenF(true, true, COLOR_STD_BG);
                            cursor = 1;
                            scroll = 0;
                            break;
                        } else ShowPrompt(false, "%s", STR_FAILED_SETTING_UP_TITLE_MANAGER);
                    }
                } else if (user_select == language) {
                    if (!CheckSupportDir(LANGUAGES_DIR)) {
                        ShowPrompt(false, STR_LANGUAGES_DIRECTORY_NOT_FOUND, LANGUAGES_DIR);
                    } else if (LanguageMenu(loadpath, STR_HOME_LANGUAGE_MENU_SELECT_LANGUAGE)) {
                        size_t fsize = FileGetSize(loadpath);
                        if (fsize > 0) {
                            char* data = (char*)malloc(fsize);
                            if (data) {
                                FileGetData(loadpath, data, fsize, 0);
                                SaveSupportFile("language.trf", data, fsize);
                                SetLanguage(data, fsize);
                                free(data);
                            }

                            // Try load font with the same name
                            char *ext = strstr(loadpath, ".trf");
                            strcpy(ext, ".frf");
                            fsize = FileGetSize(loadpath);
                            if (fsize > 0) {
                                char* data = (char*)malloc(fsize);
                                if (data) {
                                    FileGetData(loadpath, data, fsize, 0);
                                    SaveSupportFile("font.frf", data, fsize);
                                    SetFont(data, fsize);
                                    free(data);
                                }
                            }
                        }
                        GetDirContents(current_dir, current_path);
                        ClearScreenF(true, true, COLOR_STD_BG);
                        break;
                    }
                } else if (user_select == scripts) {
                    if (!CheckSupportDir(SCRIPTS_DIR)) {
                        ShowPrompt(false, STR_SCRIPTS_DIRECTORY_NOT_FOUND, SCRIPTS_DIR);
                    } else if (FileSelectorSupport(loadpath, STR_HOME_SCRIPTS_MENU_SELECT_SCRIPT, SCRIPTS_DIR, "*.gm9")) {
                        ExecuteGM9Script(loadpath);
                        GetDirContents(current_dir, current_path);
                        ClearScreenF(true, true, COLOR_STD_BG);
                        break;
                    }
#ifndef NO_LUA
                } else if (user_select == luascripts) {
                    if (!CheckSupportDir(LUASCRIPTS_DIR)) {
                        ShowPrompt(false, STR_LUA_SCRIPTS_DIRECTORY_NOT_FOUND, LUASCRIPTS_DIR);
                    } else if (FileSelectorSupport(loadpath, STR_HOME_LUA_SCRIPTS_MENU_SELECT_SCRIPT, LUASCRIPTS_DIR, "*.lua")) {
                        ExecuteLuaScript(loadpath);
                        GetDirContents(current_dir, current_path);
                        ClearScreenF(true, true, COLOR_STD_BG);
                        break;
                    }
#endif
                } else if (user_select == payloads) {
                    if (!CheckSupportDir(PAYLOADS_DIR)) ShowPrompt(false, STR_PAYLOADS_DIRECTORY_NOT_FOUND, PAYLOADS_DIR);
                    else if (FileSelectorSupport(loadpath, STR_HOME_PAYLOADS_MENU_SELECT_PAYLOAD, PAYLOADS_DIR, "*.firm"))
                        BootFirmHandler(loadpath, false, false);
                } else if (user_select == brick) {
                    Paint9(); // hiding a secret here
                    ClearScreenF(true, true, COLOR_STD_BG);
                    break;
                }
            }

            if (user_select == poweroff) {
                exit_mode = GODMODE_EXIT_POWEROFF;
                break;
            } else if (user_select == reboot) {
                exit_mode = GODMODE_EXIT_REBOOT;
                break;
            }
        } else if (pad_state & (CART_INSERT|CART_EJECT)) {
            if (!InitVCartDrive() && (pad_state & CART_INSERT) &&
                (curr_drvtype & DRV_CART)) // reinit virtual cart drive
                ShowPrompt(false, "%s", STR_CART_INIT_FAILED);
            if (!(*current_path) || (curr_drvtype & DRV_CART))
                GetDirContents(current_dir, current_path); // refresh dir contents
        } else if (pad_state & SD_INSERT) {
            while (!InitSDCardFS() && ShowPrompt(true, "%s", STR_INITIALIZING_SD_FAILED_RETRY));
            ClearScreenF(true, true, COLOR_STD_BG);
            AutoEmuNandBase(true);
            InitExtFS();
            GetDirContents(current_dir, current_path);
        } else if ((pad_state & SD_EJECT) && CheckSDMountState()) {
            ShowPrompt(false, "%s", STR_UNEXPECTED_SD_CARD_REMOVAL_TO_PREVENT_DATA_LOSS_UNMOUNT_BEFORE_EJECT);
            DeinitExtFS();
            DeinitSDCardFS();
            InitExtFS();
            if (clipboard->n_entries && (DriveType(clipboard->entry[0].path) &
                (DRV_SDCARD|DRV_ALIAS|DRV_EMUNAND|DRV_IMAGE)))
                clipboard->n_entries = 0; // remove SD clipboard entries
            GetDirContents(current_dir, current_path);
        }
    }


    DeinitExtFS();
    DeinitSDCardFS();

    if (current_dir) free(current_dir);
    if (clipboard) free(clipboard);
    if (panedata) free(panedata);

    return exit_mode;
}

#else
u32 ScriptRunner(int entrypoint) {
    // init font and show splash
    if (!SetFont(NULL, 0)) return GODMODE_EXIT_POWEROFF;
    SetLanguage(NULL, 0);
    SplashInit("scriptrunner mode");
    u64 timer = timer_start();

    InitSDCardFS();
    AutoEmuNandBase(true);
    InitNandCrypto(entrypoint != ENTRY_B9S);
    InitExtFS();
    if (!CalibrateTouchFromSupportFile())
        CalibrateTouchFromFlash();

    // brightness from file?
    s32 brightness = -1;
    if (LoadSupportFile("gm9bright.cfg", &brightness, 0x4))
        SetScreenBrightness(brightness);

    while (CheckButton(BOOTPAUSE_KEY)); // don't continue while these keys are held
    while (timer_msec( timer ) < 500); // show splash for at least 0.5 sec

    // you didn't really install a scriptrunner to NAND, did you?
    if (IS_UNLOCKED && (entrypoint == ENTRY_NANDBOOT))
        BootFirmHandler("0:/iderped.firm", false, false);

    if (PathExist("V:/" VRAM0_AUTORUN_GM9)) {
        ClearScreenF(true, true, COLOR_STD_BG); // clear splash
        ExecuteGM9Script("V:/" VRAM0_AUTORUN_GM9);
    } else if (PathExist("V:/" VRAM0_AUTORUN_LUA)) {
        ClearScreenF(true, true, COLOR_STD_BG); // clear splash
        ExecuteLuaScript("V:/" VRAM0_AUTORUN_LUA);
    } else if (PathExist("V:/" VRAM0_SCRIPTS)) {
        char loadpath[256];
        char title[256];
        snprintf(title, sizeof(title), STR_FLAVOR_SCRIPTS_MENU_SELECT_SCRIPT, FLAVOR);
        if (FileSelector(loadpath, title, "V:/" VRAM0_SCRIPTS, "*.gm9", HIDE_EXT, false))
            ExecuteGM9Script(loadpath);
    } else if (PathExist("V:/" VRAM0_LUASCRIPTS)) {
        char loadpath[256];
        char title[256];
        snprintf(title, sizeof(title), STR_FLAVOR_SCRIPTS_MENU_SELECT_SCRIPT, FLAVOR);
        if (FileSelector(loadpath, title, "V:/" VRAM0_LUASCRIPTS, "*.lua", HIDE_EXT, false))
            ExecuteLuaScript(loadpath);
    } else ShowPrompt(false, STR_COMPILED_AS_SCRIPT_AUTORUNNER_BUT_NO_SCRIPT_DERP);

    // deinit
    DeinitExtFS();
    DeinitSDCardFS();

    return GODMODE_EXIT_REBOOT;
}
#endif
