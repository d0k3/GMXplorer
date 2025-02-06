#include "scripting.h"
#include "fs.h"
#include "utils.h"
#include "nand.h"
#include "gamecart.h"
#include "bootfirm.h"
#include "qrcodegen.h"
#include "game.h"
#include "power.h"
#include "unittype.h"
#include "region.h"
#include "rtc.h"
#include "sha.h"
#include "hid.h"
#include "ui.h"
#include "swkbd.h"
#include "png.h"
#include "ips.h"
#include "bps.h"
#include "pxi.h"


#define _MAX_ARGS       4
#define _ARG_MAX_LEN    512
#define _VAR_CNT_LEN    256
#define _VAR_NAME_LEN   32
#define _VAR_MAX_BUFF   256
#define _ERR_STR_LEN    256

#define _CHOICE_STR_LEN 32
#define _CHOICE_MAX_N   12

#define _CMD_NOT        "not"
#define _CMD_IF         "if"
#define _CMD_ELIF       "elif"
#define _CMD_ELSE       "else"
#define _CMD_END        "end"
#define _CMD_FOR        "for"
#define _CMD_NEXT       "next"

#define _ARG_TRUE       "TRUE"
#define _ARG_FALSE      "FALSE"
#define _VAR_FORPATH    "FORPATH"

#define _SKIP_BLOCK     1
#define _SKIP_TILL_END  2
#define _SKIP_TO_NEXT   3
#define _SKIP_TO_FOR    4

#define _MAX_FOR_DEPTH  16

// macros for textviewer
#define TV_VPAD         1 // vertical padding per line (above / below)
#define TV_HPAD         0 // horizontal padding per line (left)
#define TV_LNOS         4 // # of digits in line numbers (0 to disable)

#define TV_NLIN_DISP    (SCREEN_HEIGHT / (FONT_HEIGHT_EXT + (2*TV_VPAD)))
#define TV_LLEN_DISP    (((SCREEN_WIDTH_TOP - (2*TV_HPAD)) / FONT_WIDTH_EXT) - (TV_LNOS + 1))

// some useful macros
#define IS_WHITESPACE(c)    ((c == ' ') || (c == '\t') || (c == '\r') || (c == '\n'))
#define MATCH_STR(s,l,c)    ((l == strlen(c)) && (strncmp(s, c, l) == 0))
#define _FLG(c)             ((c >= 'a') ? (1 << (c - 'a')) : ((c >= '0') ? (1 << (26 + c - '0')) : 0))

#define IS_CTRLFLOW_CMD(id) ((id == CMD_ID_IF) || (id == CMD_ID_ELIF) || (id == CMD_ID_ELSE) || (id == CMD_ID_END) || \
    (id == CMD_ID_GOTO) || (id == CMD_ID_LABELSEL) || \
    (id == CMD_ID_FOR) || (id == CMD_ID_NEXT))

// command ids (also entry into the cmd_list array below)
typedef enum {
    CMD_ID_NONE = 0,
    CMD_ID_NOT,
    CMD_ID_IF,
    CMD_ID_ELIF,
    CMD_ID_ELSE,
    CMD_ID_END,
    CMD_ID_FOR,
    CMD_ID_NEXT,
    CMD_ID_GOTO,
    CMD_ID_LABELSEL,
    CMD_ID_KEYCHK,
    CMD_ID_ECHO,
    CMD_ID_QR,
    CMD_ID_ASK,
    CMD_ID_INPUT,
    CMD_ID_FILESEL,
    CMD_ID_DIRSEL,
    CMD_ID_SET,
    CMD_ID_STRSPLIT,
    CMD_ID_STRREP,
    CMD_ID_CHK,
    CMD_ID_ALLOW,
    CMD_ID_CP,
    CMD_ID_MV,
    CMD_ID_INJECT,
    CMD_ID_FILL,
    CMD_ID_FDUMMY,
    CMD_ID_RM,
    CMD_ID_MKDIR,
    CMD_ID_MOUNT,
    CMD_ID_UMOUNT,
    CMD_ID_FIND,
    CMD_ID_FINDNOT,
    CMD_ID_FGET,
    CMD_ID_FSET,
    CMD_ID_SHA,
    CMD_ID_SHAGET,
    CMD_ID_DUMPTXT,
    CMD_ID_FIXCMAC,
    CMD_ID_VERIFY,
    CMD_ID_DECRYPT,
    CMD_ID_ENCRYPT,
    CMD_ID_BUILDCIA,
    CMD_ID_INSTALL,
    CMD_ID_EXTRCODE,
    CMD_ID_CMPRCODE,
    CMD_ID_SDUMP,
    CMD_ID_APPLYIPS,
    CMD_ID_APPLYBPS,
    CMD_ID_APPLYBPM,
    CMD_ID_TEXTVIEW,
    CMD_ID_CARTDUMP,
    CMD_ID_ISDIR,
    CMD_ID_EXIST,
    CMD_ID_BOOT,
    CMD_ID_SWITCHSD,
    CMD_ID_NEXTEMU,
    CMD_ID_REBOOT,
    CMD_ID_POWEROFF,
    CMD_ID_BKPT,
} cmd_id;

typedef struct {
    cmd_id id;
    char cmd[16];
    u32 n_args;
    u32 allowed_flags;
} Gm9ScriptCmd;

typedef struct {
    char name[_VAR_NAME_LEN]; // variable name
    char content[_VAR_CNT_LEN];
} Gm9ScriptVar;

static const Gm9ScriptCmd cmd_list[] = {
    { CMD_ID_NONE    , "#"       , 0, 0 }, // dummy entry
    { CMD_ID_NOT     , _CMD_NOT  , 0, 0 }, // inverts the output of the following command
    { CMD_ID_IF      , _CMD_IF   , 1, 0 }, // control flow commands at the top of the list
    { CMD_ID_ELIF    , _CMD_ELIF , 1, 0 },
    { CMD_ID_ELSE    , _CMD_ELSE , 0, 0 },
    { CMD_ID_END     , _CMD_END  , 0, 0 },
    { CMD_ID_FOR     , _CMD_FOR  , 2, _FLG('r') },
    { CMD_ID_NEXT    , _CMD_NEXT , 0, 0 },
    { CMD_ID_GOTO    , "goto"    , 1, 0 },
    { CMD_ID_LABELSEL, "labelsel", 2, _FLG('k') },
    { CMD_ID_KEYCHK  , "keychk"  , 1, 0 },
    { CMD_ID_ECHO    , "echo"    , 1, 0 },
    { CMD_ID_QR      , "qr"      , 2, 0 },
    { CMD_ID_ASK     , "ask"     , 1, 0 },
    { CMD_ID_INPUT   , "input"   , 2, 0 },
    { CMD_ID_FILESEL , "filesel" , 3, _FLG('d') | _FLG('x') },
    { CMD_ID_DIRSEL  , "dirsel"  , 3, _FLG('x') },
    { CMD_ID_SET     , "set"     , 2, 0 },
    { CMD_ID_STRSPLIT, "strsplit", 3, _FLG('b') | _FLG('f')},
    { CMD_ID_STRREP  , "strrep"  , 3, 0 },
    { CMD_ID_CHK     , "chk"     , 2, _FLG('u') },
    { CMD_ID_ALLOW   , "allow"   , 1, _FLG('a') },
    { CMD_ID_CP      , "cp"      , 2, _FLG('h') | _FLG('1') | _FLG('w') | _FLG('k') | _FLG('s') | _FLG('n') | _FLG('p')},
    { CMD_ID_MV      , "mv"      , 2, _FLG('w') | _FLG('k') | _FLG('s') | _FLG('n') },
    { CMD_ID_INJECT  , "inject"  , 2, _FLG('n') },
    { CMD_ID_FILL    , "fill"    , 2, _FLG('n') },
    { CMD_ID_FDUMMY  , "fdummy"  , 2, 0 },
    { CMD_ID_RM      , "rm"      , 1, 0 },
    { CMD_ID_MKDIR   , "mkdir"   , 1, 0 },
    { CMD_ID_MOUNT   , "imgmount", 1, 0 },
    { CMD_ID_UMOUNT  , "imgumount",0, 0 },
    { CMD_ID_FIND    , "find"    , 2, _FLG('f') },
    { CMD_ID_FINDNOT , "findnot" , 2, 0 },
    { CMD_ID_FGET    , "fget"    , 2, _FLG('e') },
    { CMD_ID_FSET    , "fset"    , 2, _FLG('e') },
    { CMD_ID_SHA     , "sha"     , 2, _FLG('1') },
    { CMD_ID_SHAGET  , "shaget"  , 2, _FLG('1') },
    { CMD_ID_DUMPTXT , "dumptxt" , 2, _FLG('p') },
    { CMD_ID_FIXCMAC , "fixcmac" , 1, 0 },
    { CMD_ID_VERIFY  , "verify"  , 1, 0 },
    { CMD_ID_DECRYPT , "decrypt" , 1, 0 },
    { CMD_ID_ENCRYPT , "encrypt" , 1, 0 },
    { CMD_ID_BUILDCIA, "buildcia", 1, _FLG('l') },
    { CMD_ID_INSTALL , "install" , 1, _FLG('e') },
    { CMD_ID_EXTRCODE, "extrcode", 2, 0 },
    { CMD_ID_CMPRCODE, "cmprcode", 2, 0 },
    { CMD_ID_SDUMP   , "sdump"   , 1, _FLG('w') },
    { CMD_ID_APPLYIPS, "applyips", 3, 0 },
    { CMD_ID_APPLYBPS, "applybps", 3, 0 },
    { CMD_ID_APPLYBPM, "applybpm", 3, 0 },
    { CMD_ID_TEXTVIEW, "textview", 1, 0 },
    { CMD_ID_CARTDUMP, "cartdump", 2, _FLG('e') },
    { CMD_ID_ISDIR   , "isdir"   , 1, 0 },
    { CMD_ID_EXIST   , "exist"   , 1, 0 },
    { CMD_ID_BOOT    , "boot"    , 1, 0 },
    { CMD_ID_SWITCHSD, "switchsd", 1, 0 },
    { CMD_ID_NEXTEMU , "nextemu" , 0, 0 },
    { CMD_ID_REBOOT  , "reboot"  , 0, 0 },
    { CMD_ID_POWEROFF, "poweroff", 0, 0 },
    { CMD_ID_BKPT    , "bkpt"    , 0, 0 }
};

// global vars for preview
static u32 preview_mode = 0; // 0 -> off 1 -> quick 2 -> full
static u32 script_color_active = 0;
static u32 script_color_comment = 0;
static u32 script_color_code = 0;

// global vars for control flow
static bool syntax_error = false;   // if true, severe error, script has to stop
static char* jump_ptr = NULL;       // next position after a jump
static char* for_ptr = NULL;        // position of the active 'for' command
static u32 skip_state = 0;          // zero, _SKIP_BLOCK, _SKIP_TILL_END
static u32 ifcnt = 0;               // current # of 'if' nesting

// script / var buffers
static void* script_buffer = NULL;
static void* var_buffer = NULL;


static inline bool isntrboot(void) {
    // taken over from Luma 3DS:
    // https://github.com/AuroraWright/Luma3DS/blob/bb5518b0f68d89bcd8efaf326355a770d5e57856/source/main.c#L58-L62
    const vu8 *bootMediaStatus = (const vu8 *) 0x1FFFE00C;
    const vu32 *bootPartitionsStatus = (const vu32 *) 0x1FFFE010;

    // shell closed, no error booting NTRCARD, NAND partitions not even considered
    return (bootMediaStatus[3] == 2) && !bootMediaStatus[1] && !bootPartitionsStatus[0] && !bootPartitionsStatus[1];
}

static inline u32 strntohex(const char* str, u8* hex, u32 len) {
    if (!len) {
        len = strlen(str);
        if (len%1) return 0;
        else len >>= 1;
    } else if (len*2 != strnlen(str, (len*2)+1)) {
        return 0;
    }
    for (u32 i = 0; i < len; i++) {
        char bytestr[2+1] = { 0 };
        u32 bytehex;
        memcpy(bytestr, str + (i*2), 2);
        if (sscanf(bytestr, "%02lx", &bytehex) != 1)
            return 0;
        hex[i] = (u8) bytehex;
    }
    return len;
}

static inline u32 hexntostr(const u8* hex, char* str, u32 len) {
    if (!len) return 0;
    for (u32 i = 0; i < len; i++)
        snprintf(str + (i<<1), 2 + 1, "%02lx", (u32) hex[i]);
    return len;
}

static inline u32 line_len(const char* text, u32 len, u32 ww, const char* line, char** eol) {
    u32 last = ((text + len) - line);
    u32 llen = 0;
    char* lf = NULL;
    char* spc = NULL;

    if (line >= (text + len))
        return 0; // early exit

    // search line feeds, spaces (only relevant for wordwrapped)
    for (llen = 0; !ww || (llen < ww); llen++) {
        if (ww && (line[llen] == ' ')) spc = (char*) (line + llen);
        if (!line[llen] || (line[llen] == '\n') || (llen >= last)) {
            lf = (char*) (line + llen);
            break;
        }
    }

    // line feed found, truncate trailing "empty" chars
    // for wordwrapped, stop line after last space (if any)
    if (lf) for (; (llen > 0) && (line[llen-1] <= ' '); llen--);
    else if (ww && spc) llen = (spc - line) + 1;

    // signal eol if required
    if (eol) *eol = lf;
    return llen;
}

static inline char* line_seek(const char* text, u32 len, u32 ww, const char* line, int add) {
    // safety checks /
    if (line < text) return NULL;
    if ((line >= (text + len)) && (add >= 0)) return (char*) line;

    if (!ww) { // non wordwrapped mode
        char* lf = ((char*) line - 1);

        // ensure we are at the start of the line
        while ((lf > text) && (*lf != '\n')) lf--;

        // handle backwards search
        for (; (add < 0) && (lf >= text); add++)
            for (lf--; (lf >= text) && (*lf != '\n'); lf--);

        // handle forwards search
        for (; (add > 0) && (lf < text + len); add--)
            for (lf++; (lf < text + len) && (*lf != '\n'); lf++);

        return lf + 1;
    } else { // wordwrapped mode
        char* l0 = (char*) line;

        // handle forwards wordwrapped search
        for (; (add > 0) && (l0 < text + len); add--) {
            char* eol = NULL;
            u32 llenww = line_len(text, len, ww, l0, &eol);
            if (eol || !llenww) l0 = line_seek(text, len, 0, l0, 1);
            else l0 += llenww;
        }

        // handle backwards wordwrapped search
        while ((add < 0) && (l0 > text)) {
            char* l1 = line_seek(text, len, 0, l0, -1);
            char* l0_minus1 = l1;
            int nlww = 0; // no of wordwrapped lines in paragraph
            for (char* ld = l1; ld < l0; ld = line_seek(text, len, ww, ld, 1), nlww++)
                l0_minus1 = ld;
            if (add + nlww < 0) { // haven't reached the desired line yet
                add += nlww;
                l0 = l1;
            } else { // reached the desired line
                l0 = (add == -1) ? l0_minus1 : line_seek(text, len, ww, l1, nlww + add);
                add = 0;
            }
        }


        return l0;
    }
}

static inline u32 get_lno(const char* text, u32 len, const char* line) {
    u32 lno = 1;

    for (u32 i = 0; i < len; i++) {
        if (line <= text + i) return lno;
        else if (text[i] == '\n') lno++;
    }

    return 0;
}

void set_preview(const char* name, const char* content) {
    if (strncmp(name, "PREVIEW_MODE", _VAR_NAME_LEN) == 0) {
        if (strncasecmp(content, "quick", _VAR_CNT_LEN) == 0) preview_mode = 1;
        else if (strncasecmp(content, "full", _VAR_CNT_LEN) == 0) preview_mode = 2;
        else preview_mode = 0xFF; // unknown preview mode
    } else if (strncmp(name, "PREVIEW_COLOR_ACTIVE", _VAR_NAME_LEN) == 0) {
        u8 rgb[4];
        if (strntohex(content, rgb, 3))
            script_color_active = rgb888_buf_to_rgb565(rgb);
    } else if (strncmp(name, "PREVIEW_COLOR_COMMENT", _VAR_NAME_LEN) == 0) {
        u8 rgb[4];
        if (strntohex(content, rgb, 3))
            script_color_comment = rgb888_buf_to_rgb565(rgb);
    } else if (strncmp(name, "PREVIEW_COLOR_CODE", _VAR_NAME_LEN) == 0) {
        u8 rgb[4];
        if (strntohex(content, rgb, 3))
            script_color_code = rgb888_buf_to_rgb565(rgb);
    }
}

char* set_var(const char* name, const char* content) {
    Gm9ScriptVar* vars = (Gm9ScriptVar*) var_buffer;

    if ((strnlen(name, _VAR_NAME_LEN) > (_VAR_NAME_LEN-1)) || (strnlen(content, _VAR_CNT_LEN) > (_VAR_CNT_LEN-1)) ||
        (strchr(name, '[') || strchr(name, ']')))
        return NULL;

    u32 n_var = 0;
    for (Gm9ScriptVar* var = vars; n_var < _VAR_MAX_BUFF; n_var++, var++)
        if (!*(var->name) || (strncmp(var->name, name, _VAR_NAME_LEN) == 0)) break;
    if (n_var >= _VAR_MAX_BUFF) return NULL;
    strncpy(vars[n_var].name, name, _VAR_NAME_LEN);
    vars[n_var].name[_VAR_NAME_LEN - 1] = '\0';
    strncpy(vars[n_var].content, content, _VAR_CNT_LEN);
    vars[n_var].content[_VAR_CNT_LEN - 1] = '\0';
    if (!n_var) *(vars[n_var].content) = '\0'; // NULL var

    // update preview stuff
    set_preview(name, content);

    return vars[n_var].content;
}

void upd_var(const char* name) {
    // device serial / region
    if (!name || (strncmp(name, "SERIAL", _VAR_NAME_LEN) == 0) ||
        (strncmp(name, "REGION", _VAR_NAME_LEN) == 0)) {
        u8 secinfo_data[1 + 1 + 16] = { 0 };
        char* env_serial = (char*) secinfo_data + 2;
        char env_region[3 + 1] = { 0 };

        snprintf(env_region, 0x4, "UNK");
        if ((FileGetData("1:/rw/sys/SecureInfo_A", secinfo_data, 0x11, 0x100) != 0x11) &&
            (FileGetData("1:/rw/sys/SecureInfo_B", secinfo_data, 0x11, 0x100) != 0x11))
            snprintf(env_serial, 0xF, "UNKNOWN");
        else if (*secinfo_data < SMDH_NUM_REGIONS)
            strncpy(env_region, g_regionNamesShort[*secinfo_data], countof(env_region) - 1);

        set_var("SERIAL", env_serial);
        set_var("REGION", env_region);
    }

    // device sysnand / emunand id0
    for (u32 emu = 0; emu <= 1; emu++) {
        const char* env_id0_name = (emu) ? "EMUID0" : "SYSID0";
        if (!name || (strncmp(name, env_id0_name, _VAR_NAME_LEN) == 0)) {
            const char* path = emu ? "4:/private/movable.sed" : "1:/private/movable.sed";
            char env_id0[32+1];
            u8 sd_keyy[0x10] __attribute__((aligned(4)));
            if (FileGetData(path, sd_keyy, 0x10, 0x110) == 0x10) {
                u32 sha256sum[8];
                sha_quick(sha256sum, sd_keyy, 0x10, SHA256_MODE);
                snprintf(env_id0, sizeof(env_id0), "%08lx%08lx%08lx%08lx",
                    sha256sum[0], sha256sum[1], sha256sum[2], sha256sum[3]);
            } else snprintf(env_id0, 0xF, "UNKNOWN");
            set_var(env_id0_name, env_id0);
        }
    }

    // datestamp & timestamp
    if (!name || (strncmp(name, "DATESTAMP", _VAR_NAME_LEN) == 0)  || (strncmp(name, "TIMESTAMP", _VAR_NAME_LEN) == 0)) {
        DsTime dstime;
        get_dstime(&dstime);
        char env_date[16+1];
        char env_time[16+1];
        snprintf(env_date, sizeof(env_date), "%02lX%02lX%02lX", (u32) dstime.bcd_Y, (u32) dstime.bcd_M, (u32) dstime.bcd_D);
        snprintf(env_time, sizeof(env_time), "%02lX%02lX%02lX", (u32) dstime.bcd_h, (u32) dstime.bcd_m, (u32) dstime.bcd_s);
        if (!name || (strncmp(name, "DATESTAMP", _VAR_NAME_LEN) == 0)) set_var("DATESTAMP", env_date);
        if (!name || (strncmp(name, "TIMESTAMP", _VAR_NAME_LEN) == 0)) set_var("TIMESTAMP", env_time);
    }

    // emunand base sector
    if (!name || (strncmp(name, "EMUBASE", _VAR_NAME_LEN) == 0)) {
        u32 emu_base = GetEmuNandBase();
        char emu_base_str[8+1];
        snprintf(emu_base_str, sizeof(emu_base_str), "%08lX", emu_base);
        set_var("EMUBASE", emu_base_str);
    }

    // SD card storage
    if (!name || (strncmp(name, "SDSIZE", _VAR_NAME_LEN) == 0)) {
        u64 sdsize = GetTotalSpace("0:");
        char sdsize_str[32+1];
        FormatBytes(sdsize_str, sdsize);
        set_var("SDSIZE", sdsize_str);
    }

    // SD card free storage
    if (!name || (strncmp(name, "SDFREE", _VAR_NAME_LEN) == 0)) {
        u64 sdfree = GetFreeSpace("0:");
        char sdfree_str[32+1];
        FormatBytes(sdfree_str, sdfree);
        set_var("SDFREE", sdfree_str);
    }

    // NAND size
    if (!name || (strncmp(name, "NANDSIZE", _VAR_NAME_LEN) == 0)) {
        u64 nandsize = GetNandSizeSectors(NAND_SYSNAND) * 0x200;
        char nandsize_str[32+1];
        FormatBytes(nandsize_str, nandsize);
        set_var("NANDSIZE", nandsize_str);
    }
}

char* get_var(const char* name, char** endptr) {
    Gm9ScriptVar* vars = (Gm9ScriptVar*) var_buffer;

    u32 name_len = 0;
    char* pname = NULL;
    if (!endptr) { // no endptr, varname is verbatim
        pname = (char*) name;
        name_len = strnlen(pname, _VAR_NAME_LEN);
    } else { // endptr given, varname is in [VAR] format
        pname = (char*) name + 1;
        if (*name != '[') return NULL;
        for (name_len = 0; pname[name_len] != ']'; name_len++)
            if ((name_len >= _VAR_NAME_LEN) || !pname[name_len]) return NULL;
        *endptr = pname + name_len + 1;
    }

    char vname[_VAR_NAME_LEN];
    strncpy(vname, pname, name_len);
    vname[name_len] = '\0';
    upd_var(vname); // handle dynamic env vars

    u32 n_var = 0;
    for (Gm9ScriptVar* var = vars; n_var < _VAR_MAX_BUFF; n_var++, var++) {
        if (!*(var->name) || (strncmp(var->name, vname, _VAR_NAME_LEN) == 0)) break;
    }

    if (n_var >= _VAR_MAX_BUFF || !*(vars[n_var].name)) n_var = 0;

    return vars[n_var].content;
}

bool init_vars(const char* path_script) {
    // reset var buffer
    memset(var_buffer, 0x00, sizeof(Gm9ScriptVar) * _VAR_MAX_BUFF);

    // current path
    char curr_dir[_VAR_CNT_LEN];
    if (path_script) {
        strncpy(curr_dir, path_script, _VAR_CNT_LEN);
        curr_dir[_VAR_CNT_LEN-1] = '\0';
        char* slash = strrchr(curr_dir, '/');
        if (slash) *slash = '\0';
    } else strncpy(curr_dir, "(null)",  _VAR_CNT_LEN - 1);

    // set env vars
    set_var("NULL", ""); // this one is special and should not be changed later
    set_var("CURRDIR", curr_dir); // script path, never changes
    set_var("GM9OUT", OUTPUT_PATH); // output path, never changes
    set_var("HAX", IS_UNLOCKED ? (isntrboot() ? "ntrboot" : "sighax") : ""); // type of hax running from
    set_var("ONTYPE", IS_O3DS ? "O3DS" : "N3DS"); // type of the console
    set_var("RDTYPE", IS_DEVKIT ? "devkit" : "retail"); // devkit / retail
    char* ptr = set_var("GM9VER", VERSION); // GodMode9 version, truncated below
    while (*(ptr++) != '\0') if (*ptr == '-') *ptr = '\0';

    return true;
}

bool expand_arg(char* argex, const char* arg, u32 len) {
    char* out = argex;

    for (char* in = (char*) arg; in - arg < (int) len; in++) {
        u32 out_len = out - argex;
        if (out_len >= (_ARG_MAX_LEN-1)) return false; // maximum arglen reached

        if (*in == '\\') { // escape line breaks & quotes
            if (*(++in) == 'n') *(out++) = '\n';
            else if (*in == '\"') *(out++) = '\"';
            else {
                *(out++) = '\\';
                *(out++) = *in;
            }
        } else if (*in == '$') { // replace vars
            char* content = get_var(in + 1, &in);
            if (content) {
                u32 clen = strnlen(content, 256);
                strncpy(out, content, clen);
                out += clen;
                in--; // go back one char
            } else *(out++) = *in;
        } else *(out++) = *in;
    }
    *out = '\0';

    return true;
}

cmd_id get_cmd_id(char* cmd, u32 len, u32 flags, u32 argc, char* err_str) {
    const Gm9ScriptCmd* cmd_entry = NULL;

    for (u32 i = 0; i < (sizeof(cmd_list)/sizeof(Gm9ScriptCmd)); i++) {
        if (strncmp(cmd_list[i].cmd, cmd, len) == 0) {
            cmd_entry = cmd_list + i;
            break;
        }
    }

    if (!cmd_entry) {
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_UNKNOWN_CMD);
    } else if (cmd_entry->n_args != argc) {
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_BAD_NUMBER_OF_ARGS);
    } else if (~(cmd_entry->allowed_flags|_FLG('o')|_FLG('s')) & flags) {
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_UNRECOGNIZED_FLAGS);
    } else return cmd_entry->id;

    return CMD_ID_NONE;
}

u32 get_flag(char* str, u32 len, char* err_str) {
    char flag_char = '\0';

    if ((len < 2) || (*str != '-')) flag_char = '\0';
    else if (len == 2) flag_char = str[1];
    else if (strncmp(str, "--sha1", len) == 0) flag_char = '1';
    else if (strncmp(str, "--all", len) == 0) flag_char = 'a';
    else if (strncmp(str, "--before", len) == 0) flag_char = 'b';
    else if (strncmp(str, "--include_dirs", len) == 0) flag_char = 'd';
    else if (strncmp(str, "--encrypted", len) == 0) flag_char = 'e';
    else if (strncmp(str, "--flip_endian", len) == 0) flag_char = 'e';
    else if (strncmp(str, "--to_emunand", len) == 0) flag_char = 'e';
    else if (strncmp(str, "--first", len) == 0) flag_char = 'f';
    else if (strncmp(str, "--hash", len) == 0) flag_char = 'h';
    else if (strncmp(str, "--keysel", len) == 0) flag_char = 'k';
    else if (strncmp(str, "--skip", len) == 0) flag_char = 'k';
    else if (strncmp(str, "--legit", len) == 0) flag_char = 'l';
    else if (strncmp(str, "--no_cancel", len) == 0) flag_char = 'n';
    else if (strncmp(str, "--optional", len) == 0) flag_char = 'o';
    else if (strncmp(str, "--append", len) == 0) flag_char = 'p';
    else if (strncmp(str, "--recursive", len) == 0) flag_char = 'r';
    else if (strncmp(str, "--silent", len) == 0) flag_char = 's';
    else if (strncmp(str, "--unequal", len) == 0) flag_char = 'u';
    else if (strncmp(str, "--overwrite", len) == 0) flag_char = 'w';
    else if (strncmp(str, "--explorer", len) == 0) flag_char = 'x';

    if (((flag_char < 'a') || (flag_char > 'z')) && ((flag_char < '0') || (flag_char > '5'))) {
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_ILLEGAL_FLAG);
        return 0;
    }

    return _FLG(flag_char);
}

char* get_string(char* ptr, const char* line_end, u32* len, char** next, char* err_str) {
    char* str = NULL;
    *len = 0;

    // skip whitespaces
    for (; IS_WHITESPACE(*ptr) && (ptr < line_end); ptr++);
    if (ptr >= line_end) return (*next = (char*) line_end); // end reached, all whitespaces

    // handle string
    if (*ptr == '\"') { // quotes
        str = ++ptr;
        for (; ((*ptr != '\"') || (*(ptr-1) == '\\')) && (ptr < line_end); ptr++, (*len)++);
        if (ptr >= line_end) { // failed if unresolved quotes
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_UNRESOLVED_QUOTES);
            return NULL;
        }
        *next = ptr + 1;
    } else { // no quotes, no whitespace
        str = ptr;
        for (; !IS_WHITESPACE(*ptr) && (ptr < line_end); ptr++, (*len)++);
        *next = ptr;
    }

    return str;
}

char* skip_block(char* ptr, bool ignore_else, bool stop_after_end) {
    while (*ptr) {
        // store line start / line end
        char* line_start = ptr;
        char* line_end = strchr(ptr, '\n');
        if (!line_end) line_end = ptr + strlen(ptr);

        // grab first string
        char* str = NULL;
        u32 STR_SCRIPTERR_len = 0;
        if (!(str = get_string(ptr, line_end, &STR_SCRIPTERR_len, &ptr, NULL)) || (str >= line_end)) {
            // string error or empty line
            ptr = line_end + 1;
            continue;
        }

        // check string
        if (MATCH_STR(str, STR_SCRIPTERR_len, _CMD_END)) { // stop at end
            return line_start; // end of block found
        } else if (!ignore_else && MATCH_STR(str, STR_SCRIPTERR_len, _CMD_ELSE)) { // stop at else
            return line_start; // end of block found
        } else if (!ignore_else && MATCH_STR(str, STR_SCRIPTERR_len, _CMD_ELIF)) { // stop at elif
            return line_start; // end of block found
        } else if (MATCH_STR(str, STR_SCRIPTERR_len, _CMD_IF)) {
            ptr = line_start = skip_block(line_end + 1, true, false);
            if (ptr == NULL) return NULL;

            line_end = strchr(ptr, '\n');
            if (!line_end) line_end = ptr + strlen(ptr);

            str = get_string(ptr, line_end, &STR_SCRIPTERR_len, &ptr, NULL);
            if (!(MATCH_STR(str, STR_SCRIPTERR_len, _CMD_END))) return NULL;
            if (stop_after_end) return line_end + 1;
        }

        // move on to the next line
        ptr = line_end + 1;
    }

    // end of block not found
    return NULL;
}

char* find_next(char* ptr) {
    while (ptr && *ptr) {
        // store line start / line end
        char* line_start = ptr;
        char* line_end = strchr(ptr, '\n');
        if (!line_end) line_end = ptr + strlen(ptr);

        // grab first string
        char* str = NULL;
        u32 STR_SCRIPTERR_len = 0;
        if (!(str = get_string(ptr, line_end, &STR_SCRIPTERR_len, &ptr, NULL)) || (str >= line_end)) {
            // string error or empty line
            ptr = line_end + 1;
            continue;
        }

        // check string
        if (MATCH_STR(str, STR_SCRIPTERR_len, _CMD_IF)) { // skip 'if' blocks
            ptr = skip_block(ptr, true, true);
        } else if (MATCH_STR(str, STR_SCRIPTERR_len, _CMD_END) || MATCH_STR(str, STR_SCRIPTERR_len, _CMD_FOR)) {
            ptr = NULL; // this should not happen here
        } else if (MATCH_STR(str, STR_SCRIPTERR_len, _CMD_NEXT)) {
            return line_start;
        }

        // move on to the next line
        ptr = line_end + 1;
    }

    // 'next' not found
    return NULL;
}

char* find_label(const char* label, const char* last_found) {
    char* script = (char*) script_buffer;
    char* ptr = script;
    u32 label_len = strnlen(label, _ARG_MAX_LEN);

    if (last_found) {
        ptr = strchr(last_found, '\n');
        if (!ptr) return NULL;
        ptr++;
    }

    char* next = ptr;
    for (; next && *ptr; ptr = next) {
        // store line start / get line end
        char* line_start = ptr;
        char* line_end = strchr(ptr, '\n');
        if (!line_end) line_end = ptr + strlen(ptr);
        next = line_end + 1;

        // search for label
        char* str = NULL;
        u32 STR_SCRIPTERR_len = 0;
        if (!(str = get_string(ptr, line_end, &STR_SCRIPTERR_len, &ptr, NULL))) continue; // string error, ignore line
        else if (str >= line_end) continue; // empty line

        if (*str == '@') {
            // label found
            str++; STR_SCRIPTERR_len--;

            // compare it manually (also check for '*' at end)
            u32 pdiff = 0;
            for (; (pdiff < STR_SCRIPTERR_len) && (label[pdiff] == str[pdiff]); pdiff++);
            if ((pdiff < label_len) && (label[pdiff] != '*')) continue; // no match
            // otherwise: potential regular or wildcard match

            // may be a match, see if there are more strings after it
            if (!(str = get_string(ptr, line_end, &STR_SCRIPTERR_len, &ptr, NULL))) continue; // string error, ignore line
            else if ((str < line_end) && (*str != '#')) continue; // neither end of line nor comment

            return line_start; // match found
        } else if (MATCH_STR(str, STR_SCRIPTERR_len, _CMD_IF)) {
            next = skip_block(line_start, true, true);
        } else if (MATCH_STR(str, STR_SCRIPTERR_len, _CMD_FOR)) {
            next = find_next(line_start);
        } // otherwise: irrelevant line
    }

    return NULL;
}

bool for_handler(char* path, const char* dir, const char* pattern, bool recursive) {
    static DIR fdir[_MAX_FOR_DEPTH];
    static DIR* dp = NULL;
    static char ldir[256];
    static char lpattern[64];
    static bool rec = false;

    if (!path && !dir && !pattern) { // close all dirs
        while (dp >= fdir) fvx_closedir(dp--);
        dp = NULL;
        return true;
    }

    if (dir) { // open a dir
        snprintf(lpattern, sizeof(lpattern), "%s", pattern);
        snprintf(ldir, sizeof(ldir), "%s", dir);
        if (dp) return false; // <- this should never happen
        if (fvx_opendir(&fdir[0], dir) != FR_OK)
            return false;
        dp = &fdir[0];
        rec = recursive;
    } else if (dp) { // traverse dir
        FILINFO fno;
        while ((fvx_preaddir(dp, &fno, lpattern) != FR_OK) || !*(fno.fname)) {
            *path = '\0';
            if (dp == fdir) return true;
            fvx_closedir(dp--);
            char* slash = strrchr(ldir, '/');
            if (!slash) return false;
            *slash = '\0';
        }

        snprintf(path, 256, "%s/%.254s", ldir, fno.fname);
        if (rec && (fno.fattrib & AM_DIR) && (dp - fdir < _MAX_FOR_DEPTH - 1)) {
            if (fvx_opendir(++dp, path) != FR_OK) dp--;
            else strncpy(ldir, path, 256);
        }
    } else return false;

    return true;
}

bool parse_line(const char* line_start, const char* line_end, cmd_id* cmdid, u32* flags, u32* argc, char** argv, char* err_str) {
    char* ptr = (char*) line_start;
    char* str;
    u32 len;

    // set everything to initial values
    *cmdid = 0;
    *flags = 0;
    *argc = 0;

    // search for cmd
    char* cmd = NULL;
    u32 cmd_len = 0;
    if (!(cmd = get_string(ptr, line_end, &cmd_len, &ptr, err_str))) return false; // string error
    if ((cmd >= line_end) || (*cmd == '#') || (*cmd == '@')) return true; // empty line or comment or label

    // special handling for "if", "elif" and "not"
    if (MATCH_STR(cmd, cmd_len, _CMD_NOT)) {
        *cmdid = CMD_ID_NOT;
        return true;
    } else if (MATCH_STR(cmd, cmd_len, _CMD_IF)) {
        *cmdid = CMD_ID_IF;
        return true;
    } else if (MATCH_STR(cmd, cmd_len, _CMD_ELIF)) {
        *cmdid = CMD_ID_ELIF;
        return true;
    }

    // got cmd, now parse flags & args
    while ((str = get_string(ptr, line_end, &len, &ptr, err_str))) {
        bool in_quotes = ((ptr - str) != (int) len); // hacky
        if ((str >= line_end) || ((*str == '#') && !in_quotes)) // end of line or comment
            return (*cmdid = get_cmd_id(cmd, cmd_len, *flags, *argc, err_str));
        if ((*str == '-') && !in_quotes) { // flag
            u32 flag_add = get_flag(str, len, err_str);
            if (!flag_add) return false; // not a proper flag
            *flags |= flag_add;
        } else if (*argc >= _MAX_ARGS) {
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_TOO_MANY_ARGUMENTS);
            return false; // too many arguments
        } else if (!expand_arg(argv[(*argc)++], str, len)) {
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_ARGUMENT_EXPAND_FAILED);
            return false; // arg expand failed
        }
    }

    // end reached with a failed get_string()
    return false;
}

bool run_cmd(cmd_id id, u32 flags, char** argv, char* err_str) {
    bool ret = true; // true unless some cmd messes up

    // process arg0 @string
    u64 at_org = 0;
    u64 sz_org = 0;
    if ((id == CMD_ID_FGET) || (id == CMD_ID_FSET) || (id == CMD_ID_SHA) || (id == CMD_ID_SHAGET) || (id == CMD_ID_INJECT) || (id == CMD_ID_FILL)) {
        char* atstr_org = strrchr(argv[0], '@');
        if (atstr_org) {
            *(atstr_org++) = '\0';
            if (sscanf(atstr_org, "%llX:%llX", &at_org, &sz_org) != 2) {
                if (sscanf(atstr_org, "%llX", &at_org) != 1) at_org = 0;
                sz_org = 0;
            }
        }
    }

    // perform command
    if (id == CMD_ID_NOT) {
        // check the argument
        // "not true" or "not false"
        ret = (strncmp(argv[0], _ARG_FALSE, _ARG_MAX_LEN) == 0);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_QUOTE_NOT_AN_ERROR);
    }
    else if (id == CMD_ID_IF) {
        // check the argument
        // "if true" or "if false"
        skip_state = (strncmp(argv[0], _ARG_TRUE, _ARG_MAX_LEN) == 0) ? 0 : _SKIP_BLOCK;
        ifcnt++;

        if (syntax_error && err_str)
            snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_SYNTAX_ERROR_AFTER_IF);
        ret = !syntax_error;
    }
    else if (id == CMD_ID_ELIF) {
        // check syntax errors
        if (ifcnt == 0) {
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_ELIF_WITHOUT_IF);
            syntax_error = true;
            return false;
        }

        // skip state handling, check the argument if required
        // "if true" or "if false"
        skip_state = !skip_state ? _SKIP_TILL_END :
            ((strncmp(argv[0], _ARG_TRUE, _ARG_MAX_LEN) == 0) ? 0 : _SKIP_BLOCK);

        if (syntax_error && err_str)
            snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_SYNTAX_ERROR_AFTER_ELIF);
        ret = !syntax_error;
    }
    else if (id == CMD_ID_ELSE) {
        // check syntax errors
        if (ifcnt == 0) {
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_ELSE_WITHOUT_IF);
            syntax_error = true;
            return false;
        }

        // turn the skip state
        skip_state = skip_state ? 0 : _SKIP_TILL_END;

        ret = true;
    }
    else if (id == CMD_ID_END) {
        // check syntax errors
        if (ifcnt == 0){
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_END_WITHOUT_IF);
            syntax_error = true;
            return false;
        }

        // close last "if"
        skip_state = 0;
        ifcnt--;

        ret = true;
    }
    else if (id == CMD_ID_FOR) {
        // cheating alert(!): actually this does nothing much
        // just sets up the for_handler and skips to 'next'
        if (for_ptr) {
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_FOR_INSIDE_FOR);
            syntax_error = true;
            return false;
        } else if (!for_handler(NULL, argv[0], argv[1], flags & _FLG('r'))) {
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_DIR_NOT_FOUND);
            skip_state = _SKIP_TO_NEXT;
            ret = false;
        } else {
            skip_state = _SKIP_TO_NEXT;
            ret = true;
        }
    }
    else if (id == CMD_ID_NEXT) {
        // actual work is done here
        char* var = set_var(_VAR_FORPATH, "");
        ret = true;
        if (!for_ptr) {
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_NEXT_WITHOUT_FOR);
            syntax_error = true;
            return false;
        } else if (!var) {
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_FORPATH_ERROR);
            ret = false;
        } else {
            if (!for_handler(var, NULL, NULL, false)) *var = '\0';
            if (!*var) {
                for_handler(NULL, NULL, NULL, false); // finish for_handler
                for_ptr = NULL;
                skip_state = 0;
            } else {
                skip_state = _SKIP_TO_FOR;
            }
            ret = true;
        }
    }
    else if (id == CMD_ID_GOTO) {
        jump_ptr = find_label(argv[0], NULL);
        if (!jump_ptr) {
            ret = false;
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_LABEL_NOT_FOUND);
        }
    }
    else if (id == CMD_ID_LABELSEL) {
        const char* options[_CHOICE_MAX_N] = { NULL };
        char* options_jmp[_CHOICE_MAX_N] = { NULL };
        char options_str[_CHOICE_MAX_N][_CHOICE_STR_LEN+1];
        u32 options_keys[_CHOICE_MAX_N] = { 0 };

        char* ast = strchr(argv[1], '*');
        char* ptr = NULL;
        u32 n_opt = 0;
        while ((ptr = find_label(argv[1], ptr))) {
            options_jmp[n_opt] = ptr;

            while (*(ptr++) != '@');
            if (ast) ptr += (ast - argv[1]);

            char* choice = options_str[n_opt];
            for (u32 i = 0; i < _CHOICE_STR_LEN; choice[++i] = '\0') {
                if (IS_WHITESPACE(ptr[i])) break;
                else if (ptr[i] == '_') choice[i] = ' ';
                else choice[i] = ptr[i];
            }
            if (flags & _FLG('k')) {
                char* keystr = choice;
                for (; *choice != ' ' && *choice != '\0'; choice++);
                if (*choice != '\0') *(choice++) = '\0';
                options_keys[n_opt] = StringToButton(keystr);
                if (!options_keys[n_opt]) continue;
            }

            options[n_opt] = choice;
            if (++n_opt >= _CHOICE_MAX_N) break;
        }

        u32 result = (flags & _FLG('k')) ? ShowHotkeyPrompt(n_opt, options, options_keys, "%s", argv[0]) :
            ShowSelectPrompt(n_opt, options, "%s", argv[0]);

        if (!result) {
            ret = false;
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_USER_ABORT);
        } else jump_ptr = options_jmp[result-1];
    }
    else if (id == CMD_ID_KEYCHK) {
        ret = CheckButton(StringToButton(argv[0]));
        if (!ret && err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_KEY_NOT_PRESSED);
    }
    else if (id == CMD_ID_ECHO) {
        ShowPrompt(false, "%s", argv[0]);
    }
    else if (id == CMD_ID_QR) {
        const u32 screen_size = SCREEN_SIZE(ALT_SCREEN);
        u8* screen_copy = (u8*) malloc(screen_size);
        u8 qrcode[qrcodegen_BUFFER_LEN_MAX];
        u8 temp[qrcodegen_BUFFER_LEN_MAX];
        ret = screen_copy && qrcodegen_encodeText(argv[1], temp, qrcode, qrcodegen_Ecc_LOW,
            qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true);
        if (ret) {
            memcpy(screen_copy, ALT_SCREEN, screen_size);
            DrawQrCode(ALT_SCREEN, qrcode);
            ShowPrompt(false, "%s", argv[0]);
            memcpy(ALT_SCREEN, screen_copy, screen_size);
        } else if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_OUT_OF_MEMORY);
        free(screen_copy);
    }
    else if (id == CMD_ID_ASK) {
        ret = ShowPrompt(true, "%s", argv[0]);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_USER_ABORT);
    }
    else if (id == CMD_ID_INPUT) {
        char input[_VAR_CNT_LEN] = { 0 };
        char* var = get_var(argv[1], NULL);
        strncpy(input, var, _VAR_CNT_LEN);
        input[_VAR_CNT_LEN - 1] = '\0';
        ret = ShowKeyboardOrPrompt(input, _VAR_CNT_LEN, "%s", argv[0]);
        if (ret) set_var(argv[1], "");
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_USER_ABORT);
        if (ret) {
            ret = set_var(argv[1], input);
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_VAR_FAIL);
        }
    }
    else if ((id == CMD_ID_FILESEL) || (id == CMD_ID_DIRSEL)) {
        char choice[_VAR_CNT_LEN];
        char* var = get_var(argv[2], NULL);
        strncpy(choice, var, _VAR_CNT_LEN);
        choice[_VAR_CNT_LEN - 1] = '\0';

        char path[_VAR_CNT_LEN];
        strncpy(path, argv[1], _VAR_CNT_LEN);
        path[_VAR_CNT_LEN - 1] = '\0';
        if (strncmp(path, "Z:", 2) == 0) {
            ret = false;
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_FORBIDDEN_DRIVE);
        } else if (id == CMD_ID_FILESEL) {
            char* npattern = strrchr(path, '/');
            if (!npattern) {
                ret = false;
                if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_INVALID_PATH);
            } else {
                u32 flags_ext = (flags & _FLG('d')) ? 0 : NO_DIRS;
                *(npattern++) = '\0';
                ret = FileSelector(choice, argv[0], path, npattern, flags_ext, (flags & _FLG('x')));
                if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_FILESELECT_ABORT);
            }
        } else {
            ret = FileSelector(choice, argv[0], path, NULL, NO_FILES | SELECT_DIRS, (flags & _FLG('x')));
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_DIRSELECT_ABORT);
        }

        if (ret) {
            ret = set_var(argv[2], choice);
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_VAR_FAIL);
        }
    }
    else if (id == CMD_ID_SET) {
        ret = set_var(argv[0], argv[1]);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_SET_FAIL);
    }
    else if (id == CMD_ID_STRSPLIT) {
        char str[_ARG_MAX_LEN];
        strncpy(str, argv[1], _ARG_MAX_LEN);
        str[_ARG_MAX_LEN - 1] = '\0';

        ret = false;
        if (strlen(argv[2]) == 1) { // argv[2] must be one char
            char* found;
            if (flags & _FLG('f')) found = strchr(str, *argv[2]);
            else found = strrchr(str, *argv[2]);
            if (!found && err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_CHAR_NOT_FOUND);

            if (found) {
                if (flags & _FLG('b')) {
                    *found = '\0';
                    ret = set_var(argv[0], str);
                } else ret = set_var(argv[0], found+1);
                if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_VAR_FAIL);
            }
        } else if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_ARGV_2_IS_NOT_CHAR);
    }
    else if (id == CMD_ID_STRREP) {
        char str[_ARG_MAX_LEN];
        strncpy(str, argv[1], _ARG_MAX_LEN);
        str[_ARG_MAX_LEN - 1] = '\0';

        if (strnlen(argv[2], _ARG_MAX_LEN) != 2) {
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_ARGV_2_MUST_BE_2_CHARS);
            ret = false;
        } else {
            for (u32 i = 0; (i < _ARG_MAX_LEN) && str[i]; i++) {
                if (str[i] == argv[2][0]) str[i] = argv[2][1];
            }
            ret = set_var(argv[0], str);
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_VAR_FAIL);
        }
    }
    else if (id == CMD_ID_CHK) {
        if (flags & _FLG('u')) {
            ret = (strncasecmp(argv[0], argv[1], _VAR_CNT_LEN) != 0);
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_ARG_MATCH);
        } else {
            ret = (strncasecmp(argv[0], argv[1], _VAR_CNT_LEN) == 0);
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_NO_ARG_MATCH);
        }
    }
    else if (id == CMD_ID_ALLOW) {
        if (flags & _FLG('a')) ret = CheckDirWritePermissions(argv[0]);
        else ret = CheckWritePermissions(argv[0]);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_PERMISSION_FAIL);
    }
    else if (id == CMD_ID_CP) {
        u32 flags_ext = BUILD_PATH;
        if (flags & _FLG('h')) flags_ext |= CALC_SHA;
        if (flags & _FLG('1')) flags_ext |= USE_SHA1;
        if (flags & _FLG('n')) flags_ext |= NO_CANCEL;
        if (flags & _FLG('s')) flags_ext |= SILENT;
        if (flags & _FLG('w')) flags_ext |= OVERWRITE_ALL;
        else if (flags & _FLG('k')) flags_ext |= SKIP_ALL;
        else if (flags & _FLG('p')) flags_ext |= APPEND_ALL;
        ret = PathMoveCopy(argv[1], argv[0], &flags_ext, false);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_COPY_FAIL);
    }
    else if (id == CMD_ID_MV) {
        u32 flags_ext = BUILD_PATH;
        if (flags & _FLG('n')) flags_ext |= NO_CANCEL;
        if (flags & _FLG('s')) flags_ext |= SILENT;
        if (flags & _FLG('w')) flags_ext |= OVERWRITE_ALL;
        else if (flags & _FLG('k')) flags_ext |= SKIP_ALL;
        ret = PathMoveCopy(argv[1], argv[0], &flags_ext, true);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_MOVE_FAIL);
    }
    else if (id == CMD_ID_INJECT) {
        char* atstr_dst = strrchr(argv[1], '@');
        u64 at_dst = 0;
        if (atstr_dst) {
            *(atstr_dst++) = '\0';
            if (sscanf(atstr_dst, "%llX", &at_dst) != 1) at_dst = 0;
        } else fvx_unlink(argv[1]); // force new file when no offset is given
        u32 flags_ext = ALLOW_EXPAND;
        if (flags & _FLG('n')) flags_ext |= NO_CANCEL;
        ret = FileInjectFile(argv[1], argv[0], at_dst, at_org, sz_org, &flags_ext);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_INJECT_FAIL);
    }
    else if (id == CMD_ID_FILL) {
        u32 flags_ext = ALLOW_EXPAND;
        if (flags & _FLG('n')) flags_ext |= NO_CANCEL;
        u8 fillbyte = 0;
        if ((strnlen(argv[1], _ARG_MAX_LEN) != 2) || !strntohex(argv[1], &fillbyte, 1)) {
            ret = false;
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_FILLBYTE_FAIL);
        } else {
            if (flags & _FLG('n')) flags_ext |= NO_CANCEL;
            ret = FileSetByte(argv[0], at_org, sz_org, fillbyte, &flags_ext);
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_FILL_FAIL);
        }
    }
    else if (id == CMD_ID_FDUMMY) {
        u32 fsize;
        if (sscanf(argv[1], "%lX", &fsize) != 1) {
            ret = false;
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_BAD_FILESIZE);
        } else {
            ret = FileCreateDummy(argv[0], NULL, fsize);
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_CREATE_DUMMY_FILE);
        }
    }
    else if (id == CMD_ID_RM) {
        char pathstr[_ERR_STR_LEN];
        TruncateString(pathstr, argv[0], 24, 8);
        ShowString("Deleting %s...", pathstr);
        ret = PathDelete(argv[0]);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_REMOVE_FAIL);
    }
    else if (id == CMD_ID_MKDIR) {
        ret = (CheckWritePermissions(argv[0])) && (fvx_rmkdir(argv[0]) == FR_OK);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_MAKEDIR_FAIL);
    }
    else if (id == CMD_ID_MOUNT) {
        ret = InitImgFS(argv[0]);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_MOUNT_FAIL);
    }
    else if (id == CMD_ID_UMOUNT) {
        InitImgFS(NULL);
    }
    else if (id == CMD_ID_FIND) {
        char path[_VAR_CNT_LEN];
        u8 mode = (flags & _FLG('f')) ? FN_LOWEST : FN_HIGHEST;
        ret = (fvx_findpath(path, argv[0], mode) == FR_OK);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_FIND_FAIL);
        if (ret) {
            ret = set_var(argv[1], path);
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_VAR_FAIL);
        }
    }
    else if (id == CMD_ID_FINDNOT) {
        char path[_VAR_CNT_LEN];
        ret = (fvx_findnopath(path, argv[0]) == FR_OK);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_FINDNOT_FAIL);
        if (ret) {
            ret = set_var(argv[1], path);
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_VAR_FAIL);
        }
    }
    else if (id == CMD_ID_FGET) {
        u8 data[(_VAR_CNT_LEN-1)/2];
        if (sz_org == 0) {
            ret = false;
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_NO_SIZE_GIVEN);
        } else if (sz_org > (_VAR_CNT_LEN-1)/2) {
            ret = false;
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_FIRM_TOO_BIG);
        } else if (FileGetData(argv[0], data, sz_org, at_org) != sz_org) {
            ret = false;
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_READ_FAIL);
        } else {
            char* var = set_var(argv[1], "");
            if (!var) {
                ret = false;
                if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_VAR_FAIL);
            } else {
                if (flags & _FLG('e')) { // flip data
                    for (u32 i = 0; i < (sz_org >> 1); i++) {
                        u8 tmp = data[i];
                        data[i] = data[sz_org - 1 - i];
                        data[sz_org - 1 - i] = tmp;
                    }
                }
                ret = hexntostr(data, var, sz_org);
                if (!ret && err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_CONVERSION_FAIL);
            }
        }
    }
    else if (id == CMD_ID_FSET) {
        u8 data[(_ARG_MAX_LEN-1)/2];
        u32 len = strntohex(argv[1], data, 0);
        if (!sz_org) sz_org = len;
        if ((sz_org <= len) && (flags & _FLG('e'))) { // flip data
            for (u32 i = 0; i < (sz_org >> 1); i++) {
                u8 tmp = data[i];
                data[i] = data[sz_org - 1 - i];
                data[sz_org - 1 - i] = tmp;
            }
        }
        if (!len) {
            ret = false;
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_INVALID_DATA);
        } else if (sz_org > len) {
            ret = false;
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_SIZE_TOO_BIG);
        } else if (!FileSetData(argv[0], data, sz_org, at_org, false)) {
            ret = false;
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_WRITE_FAIL);
        }
    }
    else if (id == CMD_ID_SHA) {
        const u8 hashlen = (flags & _FLG('1')) ? 20 : 32;
        u8 hash_fil[0x20];
        u8 hash_cmp[0x20];
        if (!FileGetSha(argv[0], hash_fil, at_org, sz_org, flags & _FLG('1'))) {
            ret = false;
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_SHA_ARG0_FAIL);
        } else if ((FileGetData(argv[1], hash_cmp, hashlen, 0) != hashlen) && !strntohex(argv[1], hash_cmp, hashlen)) {
            ret = false;
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_SHA_ARG1_FAIL);
        } else {
            ret = (memcmp(hash_fil, hash_cmp, hashlen) == 0);
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_SHA_DOES_NOT_MATCH);
        }
    }
    else if (id == CMD_ID_SHAGET) {
        const u8 hashlen = (flags & _FLG('1')) ? 20 : 32;
        u8 hash_fil[0x20];
        if (!(ret = FileGetSha(argv[0], hash_fil, at_org, sz_org, flags & _FLG('1')))) {
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_SHA_ARG0_FAIL);
        } else if (!strchr(argv[1], ':')) {
            char hash_str[64+1];
            if (flags & _FLG('1'))
                snprintf(hash_str, sizeof(hash_str), "%016llX%016llX%08lX", getbe64(hash_fil + 0), getbe64(hash_fil + 8),
                getbe32(hash_fil + 16));
            else
                snprintf(hash_str, sizeof(hash_str), "%016llX%016llX%016llX%016llX", getbe64(hash_fil + 0), getbe64(hash_fil + 8),
                getbe64(hash_fil + 16), getbe64(hash_fil + 24));
            ret = set_var(argv[1], hash_str);
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_VAR_FAIL);
        } else if (!(ret = FileSetData(argv[1], hash_fil, hashlen, 0, true))) {
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_SHA_WRITE_FAIL);
        }
    }
    else if (id == CMD_ID_DUMPTXT) {
        size_t offset = 0;
        u32 len = strnlen(argv[1], _ARG_MAX_LEN);
        if (flags & _FLG('p')) offset = FileGetSize(argv[0]);
        if (!(ret = FileSetData(argv[0], argv[1], len, offset, offset == 0)) ||
            !(ret = FileSetData(argv[0], "\n", 1, offset + len, false))) {
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_WRITE_FAIL);
        }
    }
    else if (id == CMD_ID_FIXCMAC) {
        ShowString("%s", STR_FIXING_CMACS_PLEASE_WAIT);
        ret = (RecursiveFixFileCmac(argv[0]) == 0);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_FIXCMAC_FAILED);
    }
    else if (id == CMD_ID_VERIFY) {
        u64 filetype = IdentifyFileType(argv[0]);
        if (filetype & IMG_NAND) ret = (ValidateNandDump(argv[0]) == 0);
        else ret = (VerifyGameFile(argv[0]) == 0);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_VERIFICATION_FAILED);
    }
    else if (id == CMD_ID_DECRYPT) {
        u64 filetype = IdentifyFileType(argv[0]);
        if (filetype & BIN_KEYDB) ret = (CryptAesKeyDb(argv[0], true, false) == 0);
        else ret = (CryptGameFile(argv[0], true, false) == 0);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_DECRYPT_FAILED);
    }
    else if (id == CMD_ID_ENCRYPT) {
        u64 filetype = IdentifyFileType(argv[0]);
        if (filetype & BIN_KEYDB) ret = (CryptAesKeyDb(argv[0], true, true) == 0);
        else ret = (CryptGameFile(argv[0], true, true) == 0);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_ENCRYPT_FAILED);
    }
    else if (id == CMD_ID_BUILDCIA) {
        ret = (BuildCiaFromGameFile(argv[0], (flags & _FLG('l'))) == 0);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_BUILD_CIA_FAILED);
    }
    else if (id == CMD_ID_INSTALL) {
        ret = (InstallGameFile(argv[0], (flags & _FLG('e'))) == 0);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_INSTALL_GAME_FAILED);
    }
    else if (id == CMD_ID_EXTRCODE) {
        u64 filetype = IdentifyFileType(argv[0]);
        if (!FTYPE_HASCODE(filetype)) {
            ret = false;
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_DOES_NOT_CONTAIN_DOT_CODE);
        } else {
            ShowString("%s", STR_EXTRACTING_DOT_CODE);
            ret = (ExtractCodeFromCxiFile(argv[0], argv[1], NULL) == 0);
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_EXTRACT_DOT_CODE_FAILED);
        }
    }
    else if (id == CMD_ID_CMPRCODE) {
        ShowString("%s", STR_COMPRESSING_DOT_CODE);
        ret = (CompressCode(argv[0], argv[1]) == 0);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_COMPRESS_DOT_CODE_FAILED);
    }
    else if (id == CMD_ID_SDUMP) {
        ret = false;
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_BUILD_FAILED);
        if ((strncasecmp(argv[0], TIKDB_NAME_ENC, _ARG_MAX_LEN) == 0) ||
            (strncasecmp(argv[0], TIKDB_NAME_DEC, _ARG_MAX_LEN) == 0)) {
            bool tik_dec = (strncasecmp(argv[0], TIKDB_NAME_DEC, _ARG_MAX_LEN) == 0);
            if (flags & _FLG('w')) fvx_unlink(tik_dec ? OUTPUT_PATH "/" TIKDB_NAME_DEC : OUTPUT_PATH "/" TIKDB_NAME_ENC);
            if (BuildTitleKeyInfo(NULL, tik_dec, false) == 0) {
                ShowString(STR_BUILDING_TO_OUT_ARG, OUTPUT_PATH, argv[0]);
                if (((BuildTitleKeyInfo("1:/dbs/ticket.db", tik_dec, false) == 0) ||
                     (BuildTitleKeyInfo("4:/dbs/ticket.db", tik_dec, false) == 0)) &&
                    (BuildTitleKeyInfo(NULL, tik_dec, true) == 0))
                    ret = true;
            }
        } else if (strncasecmp(argv[0], SEEDINFO_NAME, _ARG_MAX_LEN) == 0) {
            if (flags & _FLG('w')) fvx_unlink(OUTPUT_PATH "/" SEEDINFO_NAME);
            if (BuildSeedInfo(NULL, false) == 0) {
                ShowString(STR_BUILDING_TO_OUT_ARG, OUTPUT_PATH, argv[0]);
                if (((BuildSeedInfo("1:", false) == 0) ||
                     (BuildSeedInfo("4:", false) == 0)) &&
                    (BuildSeedInfo(NULL, true) == 0))
                    ret = true;
            }
        } else {
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_UNKNOWN_FILE);
        }
    }
    else if (id == CMD_ID_APPLYIPS) {
        ret = (ApplyIPSPatch(argv[0], argv[1], argv[2]) == 0);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_APPLY_IPS_FAILD);
    }
    else if (id == CMD_ID_APPLYBPS) {
        ret = (ApplyBPSPatch(argv[0], argv[1], argv[2]) == 0);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_APPLY_BPS_FAILED);
    }
    else if (id == CMD_ID_APPLYBPM) {
        ret = (ApplyBPMPatch(argv[0], argv[1], argv[2]) == 0);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_APPLY_BPM_FAILED);
    }
    else if (id == CMD_ID_TEXTVIEW) {
        ret = FileTextViewer(argv[0], false);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_TEXTVIEWER_FAILED);
    }
    else if (id == CMD_ID_CARTDUMP) {
        CartData* cdata = (CartData*) malloc(sizeof(CartData));
        u8* buf = (u8*) malloc(STD_BUFFER_SIZE);
        u64 fsize;
        ret = false;
        if (!cdata || !buf) {
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_OUT_OF_MEMORY);
        } else if (sscanf(argv[1], "%llX", &fsize) != 1) {
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_BAD_DUMPSIZE);
        } else if (InitCartRead(cdata) != 0){
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_CART_INIT_FAIL);
        } else {
            SetSecureAreaEncryption(flags & _FLG('e'));
            fvx_unlink(argv[0]);
            ret = true;
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_CART_DUMP_FAILED);
            for (u64 p = 0; p < fsize; p += STD_BUFFER_SIZE) {
                u64 len = min((fsize - p), STD_BUFFER_SIZE);
                ShowProgress(p, fsize, argv[0]);
                if (!ShowProgress(p, fsize, argv[0]) ||
                    (ReadCartBytes(buf, p, len, cdata, false) != 0) ||
                    (fvx_qwrite(argv[0], buf, p, len, NULL) != FR_OK)) {
                    ret = false;
                    break;
                }
            }
        }
        free(buf);
        free(cdata);
    }
    else if (id == CMD_ID_ISDIR) {
        DIR fdir;
        if (fvx_opendir(&fdir, argv[0]) == FR_OK) {
            fvx_closedir(&fdir);
            ret = true;
        } else {
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_NOT_A_DIR);
            ret = false;
        }
    }
    else if (id == CMD_ID_EXIST) {
        if (fvx_stat(argv[0], NULL) == FR_OK) {
            ret = true;
        } else {
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_FILE_NOT_FOUND);
            ret = false;
        }
    }
    else if (id == CMD_ID_BOOT) {
        u8* firm = (u8*) malloc(FIRM_MAX_SIZE);
        if (!firm) {
            ret = false;
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_OUT_OF_MEMORY);
        } else {
            size_t firm_size = FileGetData(argv[0], firm, FIRM_MAX_SIZE, 0);
            ret = firm_size && IsBootableFirm(firm, firm_size);
            if (ret) {
                char fixpath[256] = { 0 };
                if ((*argv[0] == '0') || (*argv[0] == '1'))
                    snprintf(fixpath, sizeof(fixpath), "%s%s", (*argv[0] == '0') ? "sdmc" : "nand", argv[0] + 1);
                else strncpy(fixpath, argv[0], 256);
                fixpath[255] = '\0';
                DeinitExtFS();
                DeinitSDCardFS();
                PXI_DoCMD(PXICMD_LEGACY_BOOT, NULL, 0);
                PXI_Barrier(PXI_FIRMLAUNCH_BARRIER);
                BootFirm((FirmHeader*)(void*)firm, fixpath);
                while(1);
            } else if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_NOT_A_BOOTABLE_FIRM);
            free(firm);
        }
    }
    else if (id == CMD_ID_SWITCHSD) {
        DeinitExtFS();
        if (!(ret = CheckSDMountState())) {
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_SD_NOT_MOUNTED);
        } else {
            u32 pad_state;
            DeinitSDCardFS();
            ShowString("%s\n \n%s", argv[0], STR_EJECT_SD_CARD);
            while (!((pad_state = InputWait(0)) & (BUTTON_B|SD_EJECT)));
            if (pad_state & SD_EJECT) {
                ShowString("%s\n \n%s", argv[0], STR_INSERT_SD_CARD);
                while (!((pad_state = InputWait(0)) & (BUTTON_B|SD_INSERT)));
            }
            if (pad_state & BUTTON_B) {
                ret = false;
                if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_USER_ABORT);
            }
        }
        InitSDCardFS();
        AutoEmuNandBase(true);
        InitExtFS();
    }
    else if (id == CMD_ID_NEXTEMU) {
        DismountDriveType(DRV_EMUNAND);
        AutoEmuNandBase(false);
        InitExtFS();
    }
    else if (id == CMD_ID_REBOOT) {
        DeinitExtFS();
        DeinitSDCardFS();
        Reboot();
    }
    else if (id == CMD_ID_POWEROFF) {
        DeinitExtFS();
        DeinitSDCardFS();
        PowerOff();
    }
    else if (id == CMD_ID_BKPT) {
        bkpt;
        while(1);
    }
    else { // command not recognized / bad number of arguments
        ret = false;
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_UNKNOWN_ERROR);
    }

    if (ret && err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_COMMAND_SUCCESS);
    return ret;
}

bool run_line(const char* line_start, const char* line_end, u32* flags, char* err_str, bool if_cond) {
    char args[_MAX_ARGS][_ARG_MAX_LEN];
    char* argv[_MAX_ARGS];
    u32 argc = 0;
    cmd_id cmdid;

    // set up argv array
    for (u32 i = 0; i < _MAX_ARGS; i++)
        argv[i] = args[i];

    // flags handling (if no pointer given)
    u32 lflags;
    if (!flags) flags = &lflags;
    *flags = 0;

    // parse current line, grab cmd / flags / args
    if (!parse_line(line_start, line_end, &cmdid, flags, &argc, argv, err_str)) {
        syntax_error = true;
        return false;
    }

    // control flow command handling
    // block out of control flow commands
    if (if_cond && IS_CTRLFLOW_CMD(cmdid)) {
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_CONTROL_FLOW_ERROR);
        syntax_error = true;
        return false;
    }

    // shortcuts for "elif" / "else"
    if (((cmdid == CMD_ID_ELIF) || (cmdid == CMD_ID_ELSE)) && !skip_state) {
        skip_state = _SKIP_TILL_END;
        cmdid = 0;
    }

    // handle "if" / "elif" / "not"
    if ((cmdid == CMD_ID_IF) || (cmdid == CMD_ID_ELIF) || (cmdid == CMD_ID_NOT)) {
        // set defaults
        argc = 1;
        strncpy(argv[0], _ARG_FALSE, _ARG_MAX_LEN - 1);

        // skip to behind the command
        char* line_start_next = (char*) line_start;
        for (; IS_WHITESPACE(*line_start_next); line_start_next++);
        for (; *line_start_next && !IS_WHITESPACE(*line_start_next); line_start_next++);

        // run condition, take over result
        if (run_line(line_start_next, line_end, flags, err_str, true))
            strncpy(argv[0], _ARG_TRUE, _ARG_MAX_LEN - 1);
    }

    // run the command (if available)
    if (cmdid && !run_cmd(cmdid, *flags, argv, err_str)) {
        char* msg_fail = get_var("ERRORMSG", NULL);
        if (msg_fail && *msg_fail) *err_str = '\0'; // use custom error message
        return false;
    }

    // success if we arrive here
    return true;
}

// checks for illegal ASCII symbols
bool ValidateText(const char* text, u32 len) {
    if (!len) return false;
    for (u32 i = 0; i < len; i++) {
        char c = text[i];
        if ((c == '\r') && ((i+1) < len) && (text[i+1] != '\n')) return false; // CR without LF
        if ((c < 0x20) && (c != '\t') && (c != '\r') && (c != '\n')) return false; // illegal control char
        if (c == 0xFF) return false; // 0xFF illegal char
    }
    return true;
}

void MemTextView(const char* text, u32 len, char* line0, int off_disp, int lno, u32 ww, u32 mno, bool is_script) {
    // block placements
    const char* al_str = "<< ";
    const char* ar_str = " >>";
    u32 x_txt = (TV_LNOS >= 0) ? TV_HPAD + ((TV_LNOS+1)*FONT_WIDTH_EXT) : TV_HPAD;
    u32 x_lno = TV_HPAD;
    u32 p_al = 0;
    u32 p_ar = TV_LLEN_DISP - strlen(ar_str);
    u32 x_al = x_txt + (p_al * FONT_WIDTH_EXT);
    u32 x_ar = x_txt + (p_ar * FONT_WIDTH_EXT);

    // display text on screen
    char txtstr[TV_LLEN_DISP + 1];
    char* ptr = line0;
    u32 nln = lno;
    for (u32 y = TV_VPAD; y < SCREEN_HEIGHT; y += FONT_HEIGHT_EXT + (2*TV_VPAD)) {
        char* ptr_next = line_seek(text, len, ww, ptr, 1);
        u32 llen = line_len(text, len, ww, ptr, NULL);
        u32 ncpy = ((int) llen < off_disp) ? 0 : (llen - off_disp);
        if (ncpy > TV_LLEN_DISP) ncpy = TV_LLEN_DISP;
        bool al = !ww && off_disp && (ptr != ptr_next);
        bool ar = !ww && (llen > off_disp + TV_LLEN_DISP);

        // set text color / find start of comment of scripts
        u32 color_text = (nln == mno) ? script_color_active : (is_script) ? script_color_code : (u32) COLOR_TVTEXT;
        int cmt_start = TV_LLEN_DISP; // start of comment in current displayed line (may be negative)
        if (is_script && (nln != mno)) {
            char* hash = line_seek(text, len, 0, ptr, 0);
            for (; *hash != '#' && (hash - ptr < (int) llen); hash++);
            cmt_start = (hash - ptr) - off_disp;
        }
        if (cmt_start <= 0) color_text = script_color_comment;

        // build text string
        snprintf(txtstr, sizeof(txtstr), "%-*.*s", (int) TV_LLEN_DISP, (int) TV_LLEN_DISP, "");
        if (ncpy) memcpy(txtstr, ptr + off_disp, ncpy);
        for (char* d = txtstr; *d; d++) if (*d < ' ') *d = ' ';
        if (al) memcpy(txtstr + p_al, al_str, strlen(al_str));
        if (ar) memcpy(txtstr + p_ar, ar_str, strlen(ar_str));

        // draw line number & text
        DrawString(TOP_SCREEN, txtstr, x_txt, y, color_text, COLOR_STD_BG);
        if (TV_LNOS > 0) { // line number
            if (ptr != ptr_next)
                DrawStringF(TOP_SCREEN, x_lno, y, ((ptr == text) || (*(ptr-1) == '\n')) ? COLOR_TVOFFS : COLOR_TVOFFSL, COLOR_STD_BG, "%0*lu", TV_LNOS, nln);
            else DrawStringF(TOP_SCREEN, x_lno, y, COLOR_TVOFFSL, COLOR_STD_BG, "%*.*s", TV_LNOS, TV_LNOS, " ");
        }

        // colorize comment if is_script
        if ((cmt_start > 0) && ((u32) cmt_start < TV_LLEN_DISP)) {
            memset(txtstr, ' ', cmt_start);
            DrawString(TOP_SCREEN, txtstr, x_txt, y, script_color_comment, COLOR_TRANSPARENT);
        }

        // colorize arrows
        if (al) DrawStringF(TOP_SCREEN, x_al, y, COLOR_TVOFFS, COLOR_TRANSPARENT, "%s", al_str);
        if (ar) DrawStringF(TOP_SCREEN, x_ar, y, COLOR_TVOFFS, COLOR_TRANSPARENT, "%s", ar_str);

        // advance pointer / line number
        for (char* c = ptr; c < ptr_next; c++) if (*c == '\n') ++nln;
        ptr = ptr_next;
    }
}

bool MemTextViewer(const char* text, u32 len, u32 start, bool as_script) {
    u32 ww = TV_LLEN_DISP;

    // check if this really is text
    if (!ValidateText(text, len)) {
        ShowPrompt(false, "%s", STR_ERROR_INVALID_TEXT_DATA);
        return false;
    }

    // clear screens
    ClearScreenF(true, true, COLOR_STD_BG);

    // instructions
    ShowString("%s", STR_TEXTVIEWER_CONTROLS_DETAILS);

    // set script colors
    if (as_script) {
        script_color_active = COLOR_TVRUN;
        script_color_comment = COLOR_TVCMT;
        script_color_code = COLOR_TVCMD;
    }

    // find maximum line len
    u32 llen_max = 0;
    for (char* ptr = (char*) text; ptr < (text + len); ptr = line_seek(text, len, 0, ptr, 1)) {
        u32 llen = line_len(text, len, 0, ptr, NULL);
        if (llen > llen_max) llen_max = llen;
    }

    // find last allowed lines (ww and nonww)
    char* llast_nww = line_seek(text, len, 0, text + len, -TV_NLIN_DISP);
    char* llast_ww = line_seek(text, len, TV_LLEN_DISP, text + len, -TV_NLIN_DISP);

    char* line0 = (char*) text;
    int lcurr = 1;
    int off_disp = 0;
    for (; lcurr < (int) start; line0 = line_seek(text, len, 0, line0, 1), lcurr++);
    while (true) {
        // display text on screen
        MemTextView(text, len, line0, off_disp, lcurr, ww, 0, as_script);

        // handle user input
        u32 pad_state = InputWait(0);
        char* line0_next = line0;
        u32 step_ud = (pad_state & BUTTON_R1) ? TV_NLIN_DISP : 1;
        u32 step_lr = (pad_state & BUTTON_R1) ? TV_LLEN_DISP : 1;
        bool switched = (pad_state & BUTTON_R1);
        if (pad_state & BUTTON_DOWN) line0_next = line_seek(text, len, ww, line0, step_ud);
        else if (pad_state & BUTTON_UP) line0_next = line_seek(text, len, ww, line0, -step_ud);
        else if (pad_state & BUTTON_RIGHT) off_disp += step_lr;
        else if (pad_state & BUTTON_LEFT) off_disp -= step_lr;
        else if (switched && (pad_state & BUTTON_X)) {
            u64 lnext64 = ShowNumberPrompt(lcurr, STR_CURRENT_LINE_N_ENTER_NEW_LINE_BELOW, lcurr);
            if (lnext64 && (lnext64 != (u64) -1)) line0_next = line_seek(text, len, 0, line0, (int) lnext64 - lcurr);
            ShowString("%s", STR_TEXTVIEWER_CONTROLS_DETAILS);
        } else if (switched && (pad_state & BUTTON_Y)) {
            ww = ww ? 0 : TV_LLEN_DISP;
            line0_next = line_seek(text, len, ww, line0, 0);
        } else if (pad_state & (BUTTON_B|BUTTON_START)) break;

        // check for problems, apply changes
        if (!ww && (line0_next > llast_nww)) line0_next = llast_nww;
        else if (ww && (line0_next > llast_ww)) line0_next = llast_ww;
        if (line0_next < line0) { // fix line number for decrease
            do if (*(--line0) == '\n') lcurr--;
            while (line0 > line0_next);
        } else { // fix line number for increase / same
            for (; line0_next > line0; line0++)
                if (*line0 == '\n') lcurr++;
        }
        if (off_disp + TV_LLEN_DISP > llen_max) off_disp = llen_max - TV_LLEN_DISP;
        if ((off_disp < 0) || ww) off_disp = 0;
    }

    // clear screens
    ClearScreenF(true, true, COLOR_STD_BG);

    return true;
}

// right now really only intended for use with the GodMode9 readme
// (misses safety checks for wider compatibility)
bool MemToCViewer(const char* text, u32 len, const char* title) {
    const u32 max_captions = 24; // we assume this is enough
    char* captions[max_captions];
    u32 lineno[max_captions];
    u32 ww = TV_LLEN_DISP;

    // check if this really is text
    if (!ValidateText(text, len)) {
        ShowPrompt(false, "%s", STR_ERROR_INVALID_TEXT_DATA);
        return false;
    }

    // clear screens / view start of readme on top
    ClearScreenF(true, true, COLOR_STD_BG);
    MemTextView(text, len, (char*) text, 0, 1, ww, 0, false);

    // parse text for markdown captions
    u32 n_captions = 0;
    char* ptr = (char*) text;
    for (u32 lno = 1;; lno++) {
        char* ptr_next = line_seek(text, len, 0, ptr, 1);
        if (ptr == ptr_next) break;
        if (*ptr == '#') {
            captions[n_captions] = ptr;
            lineno[n_captions] = lno;
            if ((lno > 1) && (++n_captions >= max_captions)) break;
        }
        ptr = ptr_next;
    }

    int cursor = -1;
    while (true) {
        // display ToC
        u32 y0 = TV_VPAD;
        u32 x0 = (SCREEN_WIDTH_BOT - GetDrawStringWidth(title)) / 2;
        DrawStringF(BOT_SCREEN, x0, y0, COLOR_TVTEXT, COLOR_STD_BG, "%s\n%*.*s", title,
            strnlen(title, 40), strnlen(title, 40), "========================================");
        y0 += 2 * (FONT_HEIGHT_EXT + (2*TV_VPAD));
        for (u32 i = 0; (i < n_captions) && (y0 < SCREEN_HEIGHT); i++) {
            u32 text_color = ((int) i == cursor) ? COLOR_TVRUN : COLOR_TVTEXT;
            char* caption = captions[i];
            u32 len = 0;
            u32 lvl = 0;
            for (; *caption == '#'; caption++, lvl++);
            for (; IS_WHITESPACE(*caption); caption++);
            for (; caption[len] != '\n' && caption[len] != '\r'; len++);
            DrawStringF(BOT_SCREEN, x0 + (lvl-1) * (FONT_WIDTH_EXT/2), y0, text_color, COLOR_STD_BG,
                "%*.*s", (int) len, (int) len, caption);
            y0 += FONT_HEIGHT_EXT + (2*TV_VPAD);
        }

        // handle user input
        u32 pad_state = InputWait(0);
        if ((cursor >= 0) && (pad_state & BUTTON_A)) {
            if (!MemTextViewer(text, len, lineno[cursor], false)) return false;
            MemTextView(text, len, captions[cursor], 0, lineno[cursor], ww, 0, false);
        } else if (pad_state & BUTTON_B) {
            break;
        } else if (pad_state & BUTTON_UP) {
            cursor = (cursor <= 0) ? ((int) n_captions - 1) : cursor - 1;
            MemTextView(text, len, captions[cursor], 0, lineno[cursor], ww, 0, false);
        } else if (pad_state & BUTTON_DOWN) {
            if (++cursor >= (int) n_captions) cursor = 0;
            MemTextView(text, len, captions[cursor], 0, lineno[cursor], ww, 0, false);
        }
    }

    // clear screens
    ClearScreenF(true, true, COLOR_STD_BG);

    return true;
}

bool FileTextViewer(const char* path, bool as_script) {
    // load text file (completely into memory)
    // text file needs to fit inside the STD_BUFFER_SIZE
    u32 flen, len;

    char* text = malloc(STD_BUFFER_SIZE);
    if (!text) return false;

    flen = FileGetData(path, text, STD_BUFFER_SIZE - 1, 0);

    text[flen] = '\0';
    len = (ptrdiff_t)memchr(text, '\0', flen + 1) - (ptrdiff_t)text;

    // let MemTextViewer take over
    bool result = MemTextViewer(text, len, 1, as_script);

    free(text);
    return result;
}

bool ExecuteGM9Script(const char* path_script) {
    char path_str[UTF_BUFFER_BYTESIZE(32)];
    TruncateString(path_str, path_script, 32, 12);


    // reset control flow global vars
    ifcnt = 0;
    jump_ptr = NULL;
    for_ptr = NULL;
    skip_state = 0;
    syntax_error = false;


    // allocate && check memory
    var_buffer = (void*) malloc(sizeof(Gm9ScriptVar) * _VAR_MAX_BUFF);
    script_buffer = (void*) malloc(SCRIPT_MAX_SIZE);
    char* script = (char*) script_buffer;
    char* ptr = script;

    if (!var_buffer || !script_buffer) {
        if (var_buffer) free(var_buffer);
        if (script_buffer) free(script_buffer);
        ShowPrompt(false, "%s", STR_OUT_OF_MEMORY);
        return false;
    }

    // fetch script from path
    u32 script_size = FileGetData(path_script, (u8*) script, SCRIPT_MAX_SIZE, 0);
    if (!script_size || (script_size >= SCRIPT_MAX_SIZE)) {
        free(var_buffer);
        free(script_buffer);
        return false;
    }

    char* end = script + script_size;
    *end = '\0';

    // initialise variables
    init_vars(path_script);

    // setup script preview (only if used)
    u32 preview_mode_local = 0;
    if (MAIN_SCREEN != TOP_SCREEN) {
        ClearScreen(TOP_SCREEN, COLOR_STD_BG);
        preview_mode = 2; // 0 -> off 1 -> quick 2 -> full
        script_color_active = COLOR_TVRUN;
        script_color_comment = COLOR_TVCMT;
        script_color_code = COLOR_TVCMD;
    }

    // script execute loop
    u32 lno = 1;
    bool result = true;
    while (ptr < end) {
        u32 flags = 0;

        // find line end
        char* line_end = strchr(ptr, '\n');
        if (!line_end) line_end = ptr + strlen(ptr);

        // update script viewer
        if (MAIN_SCREEN != TOP_SCREEN) {
            if (preview_mode != preview_mode_local) {
                if (!preview_mode || (preview_mode > 2) || !preview_mode_local)
                    ClearScreen(TOP_SCREEN, COLOR_STD_BG);
                if (preview_mode > 2) {
                    const char* preview_str = get_var("PREVIEW_MODE", NULL);
                    u32 bitmap_width, bitmap_height;
                    u16* bitmap = NULL;

                    u8* png = (u8*) malloc(SCREEN_SIZE_TOP);
                    if (png) {
                        u32 png_size = FileGetData(preview_str, png, SCREEN_SIZE_TOP, 0);
                        if (png_size && png_size < SCREEN_SIZE_TOP)
                            bitmap = PNG_Decompress(png, png_size, &bitmap_width, &bitmap_height);
                        free(png);
                    }

                    if (bitmap) {
                        DrawBitmap(TOP_SCREEN, -1, -1, bitmap_width, bitmap_height, bitmap);
                        free(bitmap);
                    } else if (ShowGameFileIcon(preview_str, TOP_SCREEN) != 0) {
                        if (strncmp(preview_str, "off", _VAR_CNT_LEN) == 0) preview_str = STR_PREVIEW_DISABLED;
                        DrawStringCenter(TOP_SCREEN, COLOR_STD_FONT, COLOR_STD_BG, "%s", preview_str);
                    }

                    preview_mode = 0;
                }
                preview_mode_local = preview_mode;
            }

            bool show_preview = preview_mode;
            if (preview_mode == 1) {
                show_preview = false;
                for (char* c = ptr; (c < line_end) && !show_preview; c++) {
                    // check for comments / labels
                    if (IS_WHITESPACE(*c)) continue;
                    else if ((*c == '#') || (*c == '@')) break;
                    else show_preview = true;
                }
            }
            if (show_preview) {
                if (lno <= (TV_NLIN_DISP/2)) {
                    MemTextView(script, script_size, script, 0, 1, 0, lno, true);
                } else {
                    char* ptr_view = line_seek(script, script_size, 0, ptr, -(TV_NLIN_DISP/2));
                    u32 lno_view = lno - (TV_NLIN_DISP/2);
                    MemTextView(script, script_size, ptr_view, 0, lno_view, 0, lno, true);
                }
            }
        }

        // run command
        char err_str[_ERR_STR_LEN+1] = { 0 };
        result = run_line(ptr, line_end, &flags, err_str, false);


        // skip state handling
        char* skip_ptr = ptr;
        if ((skip_state == _SKIP_BLOCK) || (skip_state == _SKIP_TILL_END)) {
            skip_ptr = skip_block(line_end + 1, (skip_state == _SKIP_TILL_END), false);
            if (!skip_ptr) {
                snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_UNCLOSED_CONDITIONAL);
                result = false;
                syntax_error = true;
            }
        } else if (skip_state == _SKIP_TO_NEXT) {
            skip_ptr = find_next(ptr);
            if (!skip_ptr) {
                snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_FOR_WITHOUT_NEXT);
                result = false;
                syntax_error = true;
            }
            for_ptr = (char*) line_end + 1;
        } else if (skip_state == _SKIP_TO_FOR) {
            skip_ptr = for_ptr;
            if (!skip_ptr) {
                snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_NEXT_WITHOUT_FOR);
                result = false;
                syntax_error = true;
            }
            skip_state = 0;
        }


        if (!result) { // error handling
            if (syntax_error) // severe error, can't continue
                flags &= ~(_FLG('o')|_FLG('s')); // never silent or optional

            if (!(flags & _FLG('s'))) { // not silent
                if (!*err_str) {
                    char* msg_fail = get_var("ERRORMSG", NULL);
                    if (msg_fail && *msg_fail) ShowPrompt(false, "%s", msg_fail);
                    else snprintf(err_str, _ERR_STR_LEN, "%s", STR_SCRIPTERR_ERROR_MESSAGE_FAIL);
                }
                if (*err_str) {
                    char line_str[32+1];
                    char* lptr0 = ptr;
                    char* lptr1 = line_end;
                    for (; IS_WHITESPACE(*lptr0) && (lptr0 < lptr1); lptr0++); // skip whitespaces
                    if ((lptr1 > lptr0) && (*(lptr1-1) == '\r')) lptr1--; // handle \r
                    if (lptr1 - lptr0 > 32) snprintf(line_str, sizeof(line_str), "%.29s...", lptr0);
                    else snprintf(line_str, sizeof(line_str), "%.*s", lptr1 - lptr0, lptr0);
                    ShowPrompt(false, STR_PATH_LINE_N_ERR_LINE, path_str, lno, err_str, line_str);
                }
            }
            if (!(flags & _FLG('o'))) { // failed if not optional
                for_handler(NULL, NULL, NULL, false); // make sure we don't have an open 'for'
                break;
            } else result = true; // set back the result otherwise
        }

        // reposition pointer
        if (skip_ptr != ptr) {
            ptr = skip_ptr;
            lno = get_lno(script, script_size, ptr);
        } else if (jump_ptr) {
            ptr = jump_ptr;
            lno = get_lno(script, script_size, ptr);
            ifcnt = 0; // jumping into conditional block is unexpected/unsupported
            jump_ptr = NULL;
            for_ptr = NULL;
            for_handler(NULL, NULL, NULL, false);
        } else {
            ptr = line_end + 1;
            lno++;
        }
    }


    if (result) { // all fine(?) up to this point
        if (ifcnt) { // check for unresolved 'if'
            ShowPrompt(false, "%s\n%s", path_str, STR_END_OF_SCRIPT_UNRESOLVED_IF);
            result = false;
        }
        if (for_ptr) { // check for unresolved 'for'
            ShowPrompt(false, "%s\n%s", path_str, STR_END_OF_SCRIPT_UNRESOLVED_FOR);
            for_handler(NULL, NULL, NULL, false);
            result = false;
        }
    }

    if (result) { // success message if applicable
        char* msg_okay = get_var("SUCCESSMSG", NULL);
        if (msg_okay && *msg_okay) ShowPrompt(false, "%s", msg_okay);
    }


    free(var_buffer);
    free(script_buffer);
    return result;
}
