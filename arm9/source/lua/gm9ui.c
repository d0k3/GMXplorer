#ifndef NO_LUA
#include "gm9ui.h"
#include "ui.h"
#include "fs.h"
#include "png.h"
#include "swkbd.h"
#include "qrcodegen.h"
#include "utils.h"
#include "hid.h"

#define MAXOPTIONS     256
#define MAXOPTIONS_STR "256"

#define OUTPUTMAXLINES 24
#define OUTPUTMAXCHARSPERLINE 80 // make sure this includes space for '\0'

// this output buffer stuff is especially a test, it needs to take into account newlines and fonts that are not 8x10

char output_buffer[OUTPUTMAXLINES][OUTPUTMAXCHARSPERLINE]; // hold 24 lines

void ShiftOutputBufferUp(void) {
    for (int i = 0; i < OUTPUTMAXLINES - 1; i++) {
        memcpy(output_buffer[i], output_buffer[i + 1], OUTPUTMAXCHARSPERLINE);
    }
}

void ClearOutputBuffer(void) {
    memset(output_buffer, 0, sizeof(output_buffer));
}

void WriteToOutputBuffer(char* text) {
    strlcpy(output_buffer[OUTPUTMAXLINES - 1], text, OUTPUTMAXCHARSPERLINE);
}

void RenderOutputBuffer(void) {
    ClearScreenF(false, true, COLOR_STD_BG);
    for (int i = 0; i < OUTPUTMAXLINES; i++) {
        DrawString(ALT_SCREEN, output_buffer[i], 0, i * 10, COLOR_STD_FONT, COLOR_TRANSPARENT);
    }
}

static int ui_echo(lua_State* L) {
    CheckLuaArgCount(L, 1, "ui.echo");
    const char* text = lua_tostring(L, 1);

    ShowPrompt(false, "%s", text);
    return 0;
}

static int ui_ask(lua_State* L) {
    CheckLuaArgCount(L, 1, "ui.ask");
    const char* text = lua_tostring(L, 1);

    bool ret = ShowPrompt(true, "%s", text);
    lua_pushboolean(L, ret);
    return 1;
}

static int ui_ask_hex(lua_State* L) {
    CheckLuaArgCount(L, 3, "ui.ask_hex");
    const char* text = lua_tostring(L, 1);
    u64 initial_hex = lua_tonumber(L, 2);
    u32 n_digits = lua_tonumber(L, 3);

    u64 ret = ShowHexPrompt(initial_hex, n_digits, "%s", text);
    if (ret == (u64) -1) {
        lua_pushnil(L);
    } else {
        lua_pushnumber(L, ret);
    }
    return 1;
}

static int ui_ask_number(lua_State* L) {
    CheckLuaArgCount(L, 2, "ui.ask_number");
    const char* text = lua_tostring(L, 1);
    u64 initial_num = lua_tonumber(L, 2);

    u64 ret = ShowNumberPrompt(initial_num, "%s", text);
    if (ret == (u64) -1) {
        lua_pushnil(L);
    } else {
        lua_pushnumber(L, ret);
    }
    return 1;

}

static int ui_ask_text(lua_State* L) {
    CheckLuaArgCount(L, 3, "ui.ask_text");
    const char* prompt = lua_tostring(L, 1);
    const char* _initial_text = lua_tostring(L, 2);
    u32 initial_text_size = strlen(_initial_text)+1;
    char initial_text[initial_text_size];
    snprintf(initial_text, initial_text_size, "%s", _initial_text);
    u32 max_size = lua_tonumber(L, 3);
    bool result = ShowKeyboardOrPrompt(initial_text, max_size, "%s", prompt);
    if (result)
        lua_pushstring(L, initial_text);
    else
        lua_pushnil(L);
    return 1;
}

static int ui_clear(lua_State* L) {
    CheckLuaArgCount(L, 0, "ui.clear");

    ClearScreen(ALT_SCREEN, COLOR_STD_BG);

    return 0;
}

static int ui_show_png(lua_State* L) {
    CheckLuaArgCount(L, 1, "ui.show_png");
    const char* path = luaL_checkstring(L, 1);
    u16 *screen = ALT_SCREEN;
    u16 *bitmap = NULL;
    u8* png = (u8*) malloc(SCREEN_SIZE(screen));
    u32 bitmap_width, bitmap_height;
    if (png) {
        u32 png_size = FileGetData(path, png, SCREEN_SIZE(screen), 0);
        if (!png_size) {
            free(png);
            return luaL_error(L, "Could not read %s", path);
        }
        if (png_size && png_size < SCREEN_SIZE(screen)) {
            bitmap = PNG_Decompress(png, png_size, &bitmap_width, &bitmap_height);
            if (!bitmap) {
                free(png);
                return luaL_error(L, "Invalid PNG file");
            }
        }
        free(png);
        if (!bitmap) {
            return luaL_error(L, "PNG too large");
        } else if ((SCREEN_WIDTH(screen) < bitmap_width) || (SCREEN_HEIGHT < bitmap_height)) {
            free(bitmap);
            return luaL_error(L, "PNG too large");
        } else {
            ClearScreen(ALT_SCREEN, COLOR_STD_BG);
            DrawBitmap(
                screen,        // screen
                -1,       // x coordinate from argument
                -1,       // y coordinate from argument
                bitmap_width,  // width
                bitmap_height, // height
                bitmap         // bitmap
                );
            free(bitmap);
        }
    }
    return 0;
}

static int ui_show_text(lua_State* L) {
    CheckLuaArgCount(L, 1, "ui.show_text");

    const char* text = lua_tostring(L, 1);

    ClearScreen(ALT_SCREEN, COLOR_STD_BG);
    DrawStringCenter(ALT_SCREEN, COLOR_STD_FONT, COLOR_STD_BG, "%s", text);
    return 0;
}

static int ui_show_game_info(lua_State* L) {
    CheckLuaArgCount(L, 1, "ui.show_game_info");
    const char* path = luaL_checkstring(L, 1);

    bool ret = (ShowGameFileIcon(path, ALT_SCREEN) == 0);
    if (!ret) {
        return luaL_error(L, "ShowGameFileIcon failed on %s", path);
    }

    return 0;
}

static int ui_show_qr(lua_State* L) {
    CheckLuaArgCount(L, 2, "ui.show_qr");
    size_t data_len;
    const char* text = luaL_checkstring(L, 1);
    const char* data = luaL_checklstring(L, 2, &data_len);

    const u32 screen_size = SCREEN_SIZE(ALT_SCREEN);
    u8* screen_copy = (u8*) malloc(screen_size);
    u8 qrcode[qrcodegen_BUFFER_LEN_MAX];
    u8 temp[qrcodegen_BUFFER_LEN_MAX];
    bool ret = screen_copy && qrcodegen_encodeText(data, temp, qrcode, qrcodegen_Ecc_LOW,
        qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true);
    if (ret) {
        memcpy(screen_copy, ALT_SCREEN, screen_size);
        DrawQrCode(ALT_SCREEN, qrcode);
        ShowPrompt(false, "%s", text);
        memcpy(ALT_SCREEN, screen_copy, screen_size);
    } else {
        return luaL_error(L, "could not allocate memory");
    }
    free(screen_copy);

    return 0;
}

static int ui_show_text_viewer(lua_State* L) {
    CheckLuaArgCount(L, 1, "ui.show_text_viewer");
    size_t len = 0;
    const char* text = luaL_tolstring(L, 1, &len);

    // validate text ourselves so we can return a better error
    // MemTextViewer calls ShowPrompt if it's bad, and i don't want that
    
    if (!(ValidateText(text, len))) {
        return luaL_error(L, "text validation failed");
    }

    if (!(MemTextViewer(text, len, 1, false))) {
        return luaL_error(L, "failed to run MemTextViewer");
    }

    return 0;
}

static int ui_show_file_text_viewer(lua_State* L) {
    CheckLuaArgCount(L, 1, "ui.show_file_text_viewer");
    const char* path = luaL_checkstring(L, 1);

    // validate text ourselves so we can return a better error
    // MemTextViewer calls ShowPrompt if it's bad, and i don't want that
    // and FileTextViewer calls the above function
    
    char* text = malloc(STD_BUFFER_SIZE);
    if (!text) {
        return luaL_error(L, "could not allocate memory");
    };

    // TODO: replace this with something that can detect file read errors and actual 0-length files
    size_t flen = FileGetData(path, text, STD_BUFFER_SIZE - 1, 0);

    text[flen] = '\0';
    u32 len = (ptrdiff_t)memchr(text, '\0', flen + 1) - (ptrdiff_t)text;
    
    if (!(ValidateText(text, len))) {
        free(text);
        return luaL_error(L, "text validation failed");
    }

    if (!(MemTextViewer(text, len, 1, false))) {
        free(text);
        return luaL_error(L, "failed to run MemTextViewer");
    }

    free(text);

    return 0;
}

static int ui_ask_selection(lua_State* L) {
    CheckLuaArgCount(L, 2, "ui.ask_selection");
    const char* text = lua_tostring(L, 1);
    char* options[MAXOPTIONS];
    const char* tmpstr;
    size_t len;
    int i;
    
    luaL_argcheck(L, lua_istable(L, 2), 2, "table expected");

    lua_Integer opttablesize = luaL_len(L, 2);
    luaL_argcheck(L, opttablesize <= MAXOPTIONS, 2, "more than " MAXOPTIONS_STR " options given");
    for (i = 0; i < opttablesize; i++) {
        lua_geti(L, 2, i + 1);
        tmpstr = lua_tolstring(L, -1, &len);
        options[i] = malloc(len + 1);
        strlcpy(options[i], tmpstr, len + 1);
        lua_pop(L, 1);
    }
    int result = ShowSelectPrompt(opttablesize, (const char**)options, "%s", text);
    for (i = 0; i < opttablesize; i++) free(options[i]);
    // lua only treats "false" and "nil" as false values
    // so to make this easier, return nil and not 0 if no choice was made
    // https://www.lua.org/manual/5.4/manual.html#3.3.4
    if (result)
        lua_pushinteger(L, result);
    else
        lua_pushnil(L);
    return 1;
}

static int ui_format_bytes(lua_State* L) {
    CheckLuaArgCount(L, 1, "ui.format_bytes");
    lua_Integer size = luaL_checkinteger(L, 1);

    char bytesstr[32] = { 0 };
    FormatBytes(bytesstr, (u64)size);

    lua_pushstring(L, bytesstr);
    return 1;
}

static int ui_global_print(lua_State* L) {
    //const char* text = lua_tostring(L, 1);
    char buf[OUTPUTMAXCHARSPERLINE] = {0};
    int argcount = lua_gettop(L);
    for (int i = 0; i < lua_gettop(L); i++) {
        const char* str = luaL_tolstring(L, i+1, NULL);
        if (str) {
            strlcat(buf, str, OUTPUTMAXCHARSPERLINE);
            lua_pop(L, 1);
        } else {
            // idk
            strlcat(buf, "(unknown)", OUTPUTMAXCHARSPERLINE);
        }
        if (i < argcount) strlcat(buf, " ", OUTPUTMAXCHARSPERLINE);
    }
    ShiftOutputBufferUp();
    WriteToOutputBuffer((char*)buf);
    RenderOutputBuffer();
    return 0;
}

// TODO: use luaL_checkoption which will auto-raise an error
// use BUTTON_STRINGS from common/hid_map.h
static int ui_check_key(lua_State* L) {
    CheckLuaArgCount(L, 1, "ui.check_key");
    const char* key = luaL_checkstring(L, 1);

    lua_pushboolean(L, CheckButton(StringToButton((char*)key)));

    return 1;
}

static const luaL_Reg ui_lib[] = {
    {"echo", ui_echo},
    {"ask", ui_ask},
    {"ask_hex", ui_ask_hex},
    {"ask_number", ui_ask_number},
    {"ask_text", ui_ask_text},
    {"ask_selection", ui_ask_selection},
    {"clear", ui_clear},
    {"show_png", ui_show_png},
    {"show_text", ui_show_text},
    {"show_game_info", ui_show_game_info},
    {"show_qr", ui_show_qr},
    {"show_text_viewer", ui_show_text_viewer},
    {"show_file_text_viewer", ui_show_file_text_viewer},
    {"format_bytes", ui_format_bytes},
    {"check_key", ui_check_key},
    {NULL, NULL}
};

static const luaL_Reg ui_global_lib[] = {
    {"print", ui_global_print},
    {NULL, NULL}
};

int gm9lua_open_ui(lua_State* L) {
    luaL_newlib(L, ui_lib);
    lua_pushglobaltable(L); // push global table to stack
    luaL_setfuncs(L, ui_global_lib, 0); // set global funcs
    lua_pop(L, 1); // pop global table from stack
    return 1;
}
#endif
