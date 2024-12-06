// Copyright 2013 Normmatt / 2018 d0k3
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "qrcodegen.h"
#include "vram0.h"
#include "ui.h"
#include "rtc.h"
#include "timer.h"
#include "power.h"
#include "hid.h"
#include "fixp.h"
#include "language.h"

#define STRBUF_SIZE 512 // maximum size of the string buffer
#define FONT_MAX_WIDTH 8
#define FONT_MAX_HEIGHT 10
#define PROGRESS_REFRESH_RATE 30 // the progress bar is only allowed to draw to screen every X milliseconds
#define PROGRESS_CANCEL_MIN_HOLD_MS 1000

typedef struct {
    char chunk_id[4]; // NOT null terminated
    u32 size;
} RiffChunkHeader;

typedef struct {
    u8 width;
    u8 height;
    u16 count;
} FontMeta;

static u32 font_width = 0;
static u32 font_height = 0;
static u32 font_count = 0;
static u32 line_height = 0;
static u8* font_bin = NULL;
static u16* font_map = NULL;
static u16 ascii_lut[0x60];

// lookup table to sort CP-437 so it can be binary searched with Unicode codepoints
static const u8 cp437_sorted[0x100] = {
    0x00, 0xF5, 0xF6, 0xFC, 0xFD, 0xFB, 0xFA, 0xA4, 0xF3, 0xF2, 0xF4, 0xF9, 0xF8, 0xFE, 0xFF, 0xF7,
    0xEF, 0xF1, 0xAD, 0xA5, 0x6D, 0x65, 0xED, 0xAE, 0xA9, 0xAB, 0xAA, 0xA8, 0xB2, 0xAC, 0xEE, 0xF0,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
    0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40,
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,
    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0xB8,
    0x77, 0x95, 0x85, 0x7F, 0x80, 0x7D, 0x81, 0x83, 0x86, 0x87, 0x84, 0x8B, 0x8A, 0x88, 0x74, 0x75,
    0x78, 0x82, 0x76, 0x8F, 0x90, 0x8D, 0x94, 0x92, 0x96, 0x7A, 0x7B, 0x62, 0x63, 0x64, 0xA7, 0x97,
    0x7E, 0x89, 0x8E, 0x93, 0x8C, 0x79, 0x66, 0x6F, 0x73, 0xB9, 0x68, 0x72, 0x71, 0x61, 0x67, 0x70,
    0xE9, 0xEA, 0xEB, 0xBD, 0xC3, 0xD8, 0xD9, 0xCD, 0xCC, 0xDA, 0xC8, 0xCE, 0xD4, 0xD3, 0xD2, 0xBF,
    0xC0, 0xC5, 0xC4, 0xC2, 0xBC, 0xC6, 0xD5, 0xD6, 0xD1, 0xCB, 0xE0, 0xDD, 0xD7, 0xC7, 0xE3, 0xDE,
    0xDF, 0xDB, 0xDC, 0xD0, 0xCF, 0xC9, 0xCA, 0xE2, 0xE1, 0xC1, 0xBE, 0xE6, 0xE5, 0xE7, 0xE8, 0xE4,
    0x9D, 0x7C, 0x98, 0xA0, 0x9A, 0xA1, 0x6C, 0xA2, 0x9B, 0x99, 0x9C, 0x9E, 0xB1, 0xA3, 0x9F, 0xB3,
    0xB5, 0x6A, 0xB7, 0xB6, 0xBA, 0xBB, 0x91, 0xB4, 0x69, 0xAF, 0x6E, 0xB0, 0xA6, 0x6B, 0xEC, 0x60
};

// Unicode font mapping for sorted CP-437
static const u16 cp437_sorted_map[0x100] = {
    0x0000, 0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E,
    0x002F, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E,
    0x003F, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E,
    0x004F, 0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E,
    0x005F, 0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E,
    0x006F, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E,
    0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A5, 0x00A7, 0x00AA, 0x00AB, 0x00AC, 0x00B0, 0x00B1, 0x00B2, 0x00B5, 0x00B6, 0x00B7, 0x00BA,
    0x00BB, 0x00BC, 0x00BD, 0x00BF, 0x00C4, 0x00C5, 0x00C6, 0x00C7, 0x00C9, 0x00D1, 0x00D6, 0x00DC, 0x00DF, 0x00E0, 0x00E1, 0x00E2,
    0x00E4, 0x00E5, 0x00E6, 0x00E7, 0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF, 0x00F1, 0x00F2, 0x00F3, 0x00F4,
    0x00F6, 0x00F7, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FF, 0x0192, 0x0393, 0x0398, 0x03A3, 0x03A6, 0x03A9, 0x03B1, 0x03B4, 0x03B5,
    0x03C0, 0x03C3, 0x03C4, 0x03C6, 0x2022, 0x203C, 0x207F, 0x20A7, 0x2190, 0x2191, 0x2192, 0x2193, 0x2194, 0x2195, 0x21A8, 0x2219,
    0x221A, 0x221E, 0x221F, 0x2229, 0x2248, 0x2261, 0x2264, 0x2265, 0x2302, 0x2310, 0x2320, 0x2321, 0x2500, 0x2502, 0x250C, 0x2510,
    0x2514, 0x2518, 0x251C, 0x2524, 0x252C, 0x2534, 0x253C, 0x2550, 0x2551, 0x2552, 0x2553, 0x2554, 0x2555, 0x2556, 0x2557, 0x2558,
    0x2559, 0x255A, 0x255B, 0x255C, 0x255D, 0x255E, 0x255F, 0x2560, 0x2561, 0x2562, 0x2563, 0x2564, 0x2565, 0x2566, 0x2567, 0x2568,
    0x2569, 0x256A, 0x256B, 0x256C, 0x2580, 0x2584, 0x2588, 0x258C, 0x2590, 0x2591, 0x2592, 0x2593, 0x25A0, 0x25AC, 0x25B2, 0x25BA,
    0x25BC, 0x25C4, 0x25CB, 0x25D8, 0x25D9, 0x263A, 0x263B, 0x263C, 0x2640, 0x2642, 0x2660, 0x2663, 0x2665, 0x2666, 0x266A, 0x266B
};

// lookup table to convert the old CP-437 escapes to Unicode
static const u16 cp437_escapes[0x20] = {
    0x0000, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022, 0x25D8, 0x25CB, 0x25D9, 0x2642, 0x2640, 0x266A, 0x266B, 0x263C,
    0x25BA, 0x25C4, 0x2195, 0x203C, 0x00B6, 0x00A7, 0x25AC, 0x21A8, 0x2191, 0x2193, 0x2192, 0x2190, 0x221F, 0x2194, 0x25B2, 0x25BC
};

#define PIXEL_OFFSET(x, y)  (((x) * SCREEN_HEIGHT) + (SCREEN_HEIGHT - (y) - 1))


u16 GetFontIndex(u16 c, bool use_ascii_lut)
{
    if (c < 0x20) return GetFontIndex(cp437_escapes[c], use_ascii_lut);
    else if (use_ascii_lut && c < 0x80) return ascii_lut[c - 0x20];

    int left = 0;
    int right = font_count;

    while (left <= right) {
        int mid = left + ((right - left) / 2);
        if (font_map[mid] == c)
            return mid;

        if (font_map[mid] < c)
            left = mid + 1;
        else
            right = mid - 1;
    }

    // if not found in font, return a '?'
    return ascii_lut['?' - 0x20];
}

// gets a u32 codepoint from a UTF-8 string and moves the pointer to the next character
u32 GetCharacter(const char** str)
{
    u32 c;

    if ((**str & 0x80) == 0) {
        c = *(*str)++;
    } else if ((**str & 0xE0) == 0xC0) {
        c  = (*(*str)++ & 0x1F) << 6;
        c |=  *(*str)++ & 0x3F;
    } else if ((**str & 0xF0) == 0xE0) {
        c  = (*(*str)++ & 0x0F) << 12;
        c |= (*(*str)++ & 0x3F) << 6;
        c |=  *(*str)++ & 0x3F;
    } else if ((**str & 0xF8) == 0xF0) {
        c  = (*(*str)++ & 0x07) << 18;
        c |= (*(*str)++ & 0x3F) << 12;
        c |= (*(*str)++ & 0x3F) << 6;
        c |=  *(*str)++ & 0x3F;
    } else {
        // invalid UTF-8, skip to next character
        (*str)++;
        c = '?';
    }

    return c;
}

const u8* GetFontFromPbm(const void* pbm, const u32 pbm_size, u32* w, u32* h) {
    const char* hdr = (const char*) pbm;
    u32 hdr_max_size = min(512, pbm_size);
    u32 pbm_w = 0;
    u32 pbm_h = 0;

    // minimum size
    if (hdr_max_size < 7) return NULL;

    // check header magic, then skip over
    if (strncmp(hdr, "P4\n", 3) != 0) return NULL;

    // skip any comments
    u32 p = 3;
    while (hdr[p] == '#') {
        while (hdr[p++] != '\n') {
            if (p >= hdr_max_size) return NULL;
        }
    }

    // parse width
    while ((hdr[p] >= '0') && (hdr[p] <= '9')) {
        if (p >= hdr_max_size) return NULL;
        pbm_w *= 10;
        pbm_w += hdr[p++] - '0';
    }

    // whitespace
    if ((hdr[p++] != ' ') || (p >= hdr_max_size))
        return NULL;

    // parse height
    while ((hdr[p] >= '0') && (hdr[p] <= '9')) {
        if (p >= hdr_max_size) return NULL;
        pbm_h *= 10;
        pbm_h += hdr[p++] - '0';
    }

    // line break
    if ((hdr[p++] != '\n') || (p >= hdr_max_size))
        return NULL;

    // check sizes
    if (pbm_w <= 8) { // 1x256 format
        if ((pbm_w > FONT_MAX_WIDTH) || (pbm_h % 256) ||
            ((pbm_h / 256) > FONT_MAX_HEIGHT) ||
            (pbm_h != (pbm_size - p)))
            return NULL;
    } else { // 16x16 format
        if ((pbm_w % 16) || (pbm_h % 16) ||
            ((pbm_w / 16) > FONT_MAX_WIDTH) ||
            ((pbm_h / 16) > FONT_MAX_HEIGHT) ||
            ((pbm_h * pbm_w / 8) != (pbm_size - p)))
            return NULL;
    }

    // all good
    if (w) *w = pbm_w;
    if (h) *h = pbm_h;
    return (u8*) pbm + p;
}

const u8* GetFontFromRiff(const void* riff, const u32 riff_size, u32* w, u32* h, u16* count) {
    const void* ptr = riff;
    const RiffChunkHeader* riff_header;
    const RiffChunkHeader* chunk_header;

    // check header magic and load size
    if (!ptr) return NULL;
    riff_header = ptr;
    if (memcmp(riff_header->chunk_id, "RIFF", 4) != 0) return NULL;

    // ensure enough space is allocated
    if (riff_header->size > riff_size) return NULL;

    ptr += sizeof(RiffChunkHeader);

    while ((u32)(ptr - riff) < riff_header->size + sizeof(RiffChunkHeader)) {
        chunk_header = ptr;

        // check for and load META section
        if (memcmp(chunk_header->chunk_id, "META", 4) == 0) {

            if (chunk_header->size != 4) return NULL;

            const FontMeta *meta = ptr + sizeof(RiffChunkHeader);
            if (meta->width > FONT_MAX_WIDTH || meta->height > FONT_MAX_HEIGHT) return NULL;

            // all good
            if (w) *w = meta->width;
            if (h) *h = meta->height;
            if (count) *count = meta->count;
            return ptr;
        }

        ptr += sizeof(RiffChunkHeader) + chunk_header->size;
    }

    return NULL;
}

// sets the font from a given RIFF or PBM
// if no font is given, the font is fetched from the default VRAM0 location
bool SetFont(const void* font, u32 font_size) {
    u32 w, h;
    u16 count;
    const u8* ptr = NULL;

    if (!font) {
        u64 font_size64 = 0;
        font = FindVTarFileInfo(VRAM0_FONT, &font_size64);
        font_size = (u32) font_size64;
    }

    if (!font)
        return false;

    if ((ptr = GetFontFromRiff(font, font_size, &w, &h, &count))) { // RIFF font
        font_width = w;
        font_height = h;
        font_count = count;

        const RiffChunkHeader* riff_header;
        const RiffChunkHeader* chunk_header;

        // load total size
        riff_header = font;

        while (((u32)ptr - (u32)font) < riff_header->size + sizeof(RiffChunkHeader)) {
            chunk_header = (const void *)ptr;

            if (memcmp(chunk_header->chunk_id, "CDAT", 4) == 0) { // character data
                if (font_bin) free(font_bin);
                font_bin = malloc(font_height * font_count);
                if (!font_bin) return NULL;

                memcpy(font_bin, ptr + sizeof(RiffChunkHeader), font_height * font_count);
            } else if (memcmp(chunk_header->chunk_id, "CMAP", 4) == 0) { // character map
                if (font_map) free(font_map);
                font_map = malloc(sizeof(u16) * font_count);
                if (!font_map) return NULL;

                memcpy(font_map, ptr + sizeof(RiffChunkHeader), sizeof(u16) * font_count);
            }

            ptr += sizeof(RiffChunkHeader) + chunk_header->size;
        }
    } else if ((ptr = GetFontFromPbm(font, font_size, &w, &h))) {
        font_count = 0x100;

        if (w > 8) {
            font_width = w / 16;
            font_height = h / 16;

            if (font_bin) free(font_bin);
            font_bin = malloc(font_height * font_count);
            if (!font_bin) return NULL;

            for (u32 cy = 0; cy < 16; cy++) {
                for (u32 row = 0; row < font_height; row++) {
                    for (u32 cx = 0; cx < 16; cx++) {
                        u32 bp0 = (cx * font_width) >> 3;
                        u32 bm0 = (cx * font_width) % 8;
                        u8 byte = ((ptr[bp0] << bm0) | (ptr[bp0+1] >> (8 - bm0))) & (0xFF << (8 - font_width));
                        font_bin[(cp437_sorted[(cy << 4) + cx] * font_height) + row] = byte;
                    }
                    ptr += font_width << 1;
                }
            }
        } else {
            font_width = w;
            font_height = h / 256;
            for (u32 i = 0; i < font_count; i++)
                memcpy(font_bin + cp437_sorted[i] * font_height, ptr + i * font_height, font_height);
        }

        if (font_map) free(font_map);
        font_map = malloc(sizeof(u16) * font_count);
        if (!font_map) return NULL;

        memcpy(font_map, cp437_sorted_map, sizeof(cp437_sorted_map));
    } else {
        return false;
    }


    // set up ASCII lookup table
    ascii_lut['?' - 0x20] = GetFontIndex('?', false);
    for (int i = 0; i < 0x60; i++) {
        ascii_lut[i] = GetFontIndex(i + 0x20, false);
    }

    line_height = min(10, font_height + 2);
    return true;
}

void ClearScreen(u16* screen, u32 color)
{
    u32 *screen_wide = (u32*)(void*)screen;
    int width = (screen == TOP_SCREEN) ? SCREEN_WIDTH_TOP : SCREEN_WIDTH_BOT;
    if (color == COLOR_TRANSPARENT)
        color = COLOR_BLACK;

    color |= color << 16;
    for (int i = 0; i < (width * SCREEN_HEIGHT / 2); i++)
        *(screen_wide++) = color;
}

void ClearScreenF(bool clear_main, bool clear_alt, u32 color)
{
    if (clear_main) ClearScreen(MAIN_SCREEN, color);
    if (clear_alt) ClearScreen(ALT_SCREEN, color);
}

u16 GetColor(const u16 *screen, int x, int y)
{
    return screen[PIXEL_OFFSET(x, y)];
}

void DrawPixel(u16 *screen, int x, int y, u32 color)
{
    screen[PIXEL_OFFSET(x, y)] = color;
}

void DrawRectangle(u16 *screen, int x, int y, u32 width, u32 height, u32 color)
{
    screen += PIXEL_OFFSET(x, y) - height + 1;
    while(width--) {
        for (u32 h = 0; h < height; h++)
            screen[h] = color;
        screen += SCREEN_HEIGHT;
    }
}

void DrawBitmap(u16 *screen, int x, int y, u32 w, u32 h, const u16* bitmap)
{
    // on negative values: center the bitmap
    if (x < 0) x = (SCREEN_WIDTH(screen) - w) >> 1;
    if (y < 0) y = (SCREEN_HEIGHT - h) >> 1;

    // bug out on too big bitmaps / too large dimensions
    if ((x < 0) || (y < 0) || (w > SCREEN_WIDTH(screen)) || (h > SCREEN_HEIGHT))
        return;

    screen += PIXEL_OFFSET(x, y);
    while(h--) {
        for (u32 i = 0; i < w; i++)
            screen[i * SCREEN_HEIGHT] = *(bitmap++);
        screen--;
    }
}

void DrawQrCode(u16 *screen, const u8* qrcode)
{
    const u32 size_qr = qrcodegen_getSize(qrcode);
    u32 size_qr_s = size_qr;
    u32 size_canvas = size_qr + 8;

    // handle scaling
    u32 scale = 1;
    for (; size_canvas * (scale+1) < SCREEN_HEIGHT; scale++);
    size_qr_s *= scale;
    size_canvas *= scale;

    // clear screen, draw the canvas
    u32 x_canvas = (SCREEN_WIDTH(screen) - size_canvas) / 2;
    u32 y_canvas = (SCREEN_HEIGHT - size_canvas) / 2;
    ClearScreen(screen, COLOR_STD_BG);
    DrawRectangle(screen, x_canvas, y_canvas, size_canvas, size_canvas, COLOR_WHITE);

    // draw the QR code
    u32 x_qr = (SCREEN_WIDTH(screen) - size_qr_s) / 2;
    u32 y_qr = (SCREEN_HEIGHT - size_qr_s) / 2;
    int xDisplacement = x_qr * SCREEN_HEIGHT;
    for (u32 y = 0; y < size_qr_s; y++) {
        int yDisplacement = SCREEN_HEIGHT - (y_qr + y) - 1;
        u16* screenPos = screen + xDisplacement + yDisplacement;
        for (u32 x = 0; x < size_qr_s; x++) {
            u16 c = qrcodegen_getModule(qrcode, x/scale, y/scale) ? COLOR_BLACK : COLOR_WHITE;
            *(screenPos) = c;
            screenPos += SCREEN_HEIGHT;
        }
    }
}

void DrawCharacter(u16 *screen, u32 character, int x, int y, u32 color, u32 bgcolor)
{
    for (int yy = 0; yy < (int) font_height; yy++) {
        int xDisplacement = x * SCREEN_HEIGHT;
        int yDisplacement = SCREEN_HEIGHT - (y + yy) - 1;
        u16* screenPos = screen + xDisplacement + yDisplacement;

        u8 charPos = font_bin[GetFontIndex(character, true) * font_height + yy];
        for (int xx = 7; xx >= (8 - (int) font_width); xx--) {
            if ((charPos >> xx) & 1) {
                *screenPos = color;
            } else if (bgcolor != COLOR_TRANSPARENT) {
                *screenPos = bgcolor;
            }
            screenPos += SCREEN_HEIGHT;
        }
    }
}

void DrawString(u16 *screen, const char *str, int x, int y, u32 color, u32 bgcolor)
{
    size_t max_len = (((screen == TOP_SCREEN) ? SCREEN_WIDTH_TOP : SCREEN_WIDTH_BOT) - x) / font_width;

    for (size_t i = 0; i < max_len && *str; i++) {
        DrawCharacter(screen, GetCharacter(&str), x + i * font_width, y, color, bgcolor);
    }
}

void DrawStringF(u16 *screen, int x, int y, u32 color, u32 bgcolor, const char *format, ...)
{
    char str[STRBUF_SIZE];
    va_list va;
    va_start(va, format);
    vsnprintf(str, STRBUF_SIZE, format, va);
    va_end(va);

    for (char* text = strtok(str, "\n"); text != NULL; text = strtok(NULL, "\n"), y += line_height)
        DrawString(screen, text, x, y, color, bgcolor);
}

void DrawStringCenter(u16 *screen, u32 color, u32 bgcolor, const char *format, ...)
{
    char str[STRBUF_SIZE];
    va_list va;
    va_start(va, format);
    vsnprintf(str, STRBUF_SIZE, format, va);
    va_end(va);

    u32 w = GetDrawStringWidth(str);
    u32 h = GetDrawStringHeight(str);
    int x = (w >= SCREEN_WIDTH(screen)) ? 0 : (SCREEN_WIDTH(screen) - w) >> 1;
    int y = (h >= SCREEN_HEIGHT) ? 0 : (SCREEN_HEIGHT - h) >> 1;

    DrawStringF(screen, x, y, color, bgcolor, "%s", str);
}

u32 GetDrawStringHeight(const char* str) {
    u32 height = font_height;
    for (char* lf = strchr(str, '\n'); (lf != NULL); lf = strchr(lf + 1, '\n'))
        height += line_height;
    return height;
}

u32 GetCharSize(const char* str) {
    const char *start = str;
    do {
        str++;
    } while ((*str & 0xC0) == 0x80);

    return str - start;
}

u32 GetPrevCharSize(const char* str) {
    const char *start = str;
    do {
        str--;
    } while ((*str & 0xC0) == 0x80);

    return start - str;
}

u32 GetDrawStringWidth(const char* str) {
    u32 width = 0;
    char* old_lf = (char*) str;
    char* str_end = (char*) str + strnlen(str, STRBUF_SIZE);
    for (char* lf = strchr(str, '\n'); lf != NULL; lf = strchr(lf + 1, '\n')) {
        u32 length = 0;
        for (char* c = old_lf; c != lf; c++) {
            if ((*c & 0xC0) != 0x80) length++;
        }

        if (length > width) width = length;
        old_lf = lf;
    }

    u32 length = 0;
    for (char* c = old_lf; c != str_end; c++) {
        if ((*c & 0xC0) != 0x80) length++;
    }

    if (length > width) width = length;
    width *= font_width;
    return width;
}

u32 GetFontWidth(void) {
    return font_width;
}

u32 GetFontHeight(void) {
    return font_height;
}

void MultiLineString(char* dest, const char* orig, int llen, int maxl) {
    char* ptr_o = (char*) orig;
    char* ptr_d = (char*) dest;
    for (int l = 0; l < maxl; l++) {
        int len = strnlen(ptr_o, llen+1);
        snprintf(ptr_d, llen+1, "%.*s", llen, ptr_o);
        ptr_o += min(len, llen);
        ptr_d += min(len, llen);
        if (len <= llen) break;
        *(ptr_d++) = '\n';
    }

    // string too long?
    if (!maxl) *dest = '\0';
    else if (*ptr_o) {
        if (llen >= 3) snprintf(ptr_d - 4, 4, "...");
        else *(ptr_d-1) = '\0';
    } 
}

void WordWrapString(char* str, int llen) {
    char* last_brk = str - 1;
    char* last_spc = str - 1;
    if (!llen) llen = (SCREEN_WIDTH_MAIN / font_width);
    for (char* str_ptr = str;; str_ptr++) {
        if (!*str_ptr || (*str_ptr == ' ')) { // on space or string_end
            if (str_ptr - last_brk > llen) { // if maximum line lenght is exceeded
                if (last_spc > last_brk) { // put a line_brk at the last space
                    *last_spc = '\n';
                    last_brk = last_spc;
                    last_spc = str_ptr;
                } else if (*str_ptr) { // if we have no applicable space
                    *str_ptr = '\n';
                    last_brk = str_ptr;
                }
            } else if (*str_ptr) last_spc = str_ptr;
        } else if (*str_ptr == '\n') last_brk = str_ptr;
        if (!*str_ptr) break;
    }
}

// dest must be at least 4x the size of nlength to account for UTF-8
void ResizeString(char* dest, const char* orig, int nlength, int tpos, bool align_right) {
    int olength = 0;
    for (int i = 0; i < 256 && orig[i]; i++) {
        if ((orig[i] & 0xC0) != 0x80) olength++;
    }

    if (nlength < olength) {
        TruncateString(dest, orig, nlength, tpos);
    } else {
        int nsize = 0;
        int osize = strnlen(orig, 256);
        for (int i = 0; i < nlength || (nsize <= osize && (orig[nsize] & 0xC0) == 0x80); nsize++) {
            if (nsize > osize || (orig[nsize] & 0xC0) != 0x80) i++;
        }
        snprintf(dest, UTF_BUFFER_BYTESIZE(nlength), align_right ? "%*.*s" : "%-*.*s", nsize, nsize, orig);
    }
}

// dest must be at least 4x the size of nlength to account for UTF-8
void TruncateString(char* dest, const char* orig, int nlength, int tpos) {
    int osize = strnlen(orig, 256), olength = 0;
    for (int i = 0; i < 256 && orig[i]; i++) {
        if ((orig[i] & 0xC0) != 0x80) olength++;
    }

    if (nlength < 0) {
        return;
    } else if ((nlength <= 3) || (nlength >= olength)) {
        strcpy(dest, orig);
    } else {
        if (tpos + 3 > nlength) tpos = nlength - 3;

        int tposStart = 0;
        for (int i = 0; i < tpos || (orig[tposStart] & 0xC0) == 0x80; tposStart++) {
            if ((orig[tposStart] & 0xC0) != 0x80) i++;
        }

        int tposEnd = 0;
        for (int i = 0; i < nlength - tpos - 3; tposEnd++) {
            if ((orig[osize - 1 - tposEnd] & 0xC0) != 0x80) i++;
        }

        snprintf(dest, UTF_BUFFER_BYTESIZE(nlength), "%-.*s...%-.*s", tposStart, orig, tposEnd, orig + osize - tposEnd);
    }
}

void FormatNumber(char* str, u64 number) { // str should be 32 byte in size
    u64 mag1000 = 1;
    *str = '\0';
    for (; number / (mag1000 * 1000) > 0; mag1000 *= 1000);
    for (; mag1000 > 0; mag1000 /= 1000) {
        u32 pos = strnlen(str, 31);
        snprintf(str + pos, 31 - pos, "%0*llu%s", (pos) ? 3 : 1, (number / mag1000) % 1000, (mag1000 > 1) ? STR_THOUSAND_SEPARATOR : "");
    }
}

void FormatBytes(char* str, u64 bytes) { // str should be 32 byte in size, just to be safe
    const char* units[] = {STR_BYTE, STR_KB, STR_MB, STR_GB};

    if (bytes == (u64) -1) snprintf(str, 32, "%s", STR_INVALID);
    else if (bytes < 1024) snprintf(str, 32, "%llu%s", bytes, units[0]);
    else {
        u32 scale = 1;
        u64 bytes100 = (bytes * 100) >> 10;
        for(; (bytes100 >= 1024*100) && (scale < 3); scale++, bytes100 >>= 10);
        snprintf(str, 32, "%llu%s%llu%s", bytes100 / 100, STR_DECIMAL_SEPARATOR, (bytes100 % 100) / 10, units[scale]);
    }
}

void ShowStringF(u16* screen, const char *format, ...)
{
    ClearScreen(MAIN_SCREEN, COLOR_STD_BG);
    if (format && *format) { // only if there is something in there
        char str[STRBUF_SIZE];
        va_list va;
        va_start(va, format);
        vsnprintf(str, STRBUF_SIZE, format, va);
        va_end(va);

        DrawStringCenter(screen, COLOR_STD_FONT, COLOR_STD_BG, "%s", str);
    }
}

void ShowString(const char *format, ...)
{
    ClearScreenF(true, false, COLOR_STD_BG);
    if (format && *format) { // only if there is something in there
        char str[STRBUF_SIZE];
        va_list va;
        va_start(va, format);
        vsnprintf(str, STRBUF_SIZE, format, va);
        va_end(va);

        DrawStringCenter(MAIN_SCREEN, COLOR_STD_FONT, COLOR_STD_BG, "%s", str);
    }
}

void ShowIconStringF(u16* screen, u16* icon, int w, int h, const char *format, ...)
{
    static const u32 icon_offset = 10;
    u32 str_width, str_height, tot_height;
    u32 x_str, y_str, x_bmp, y_bmp;

    ClearScreen(screen, COLOR_STD_BG);
    if (!format || !*format) return; // only if there is something in there

    char str[STRBUF_SIZE];
    va_list va;
    va_start(va, format);
    vsnprintf(str, STRBUF_SIZE, format, va);
    va_end(va);

    str_width = GetDrawStringWidth(str);
    str_height = GetDrawStringHeight(str);
    tot_height = h + icon_offset + str_height;
    x_str = (str_width >= SCREEN_WIDTH(screen)) ? 0 : (SCREEN_WIDTH(screen) - str_width) / 2;
    y_str = (str_height >= SCREEN_HEIGHT) ? 0 : h + icon_offset + (SCREEN_HEIGHT - tot_height) / 2;
    x_bmp = (w >= SCREEN_WIDTH(screen)) ? 0 : (SCREEN_WIDTH(screen) - w) / 2;
    y_bmp = (tot_height >= SCREEN_HEIGHT) ? 0 : (SCREEN_HEIGHT - tot_height) / 2;

    DrawBitmap(screen, x_bmp, y_bmp, w, h, icon);
    DrawStringF(screen, x_str, y_str, COLOR_STD_FONT, COLOR_STD_BG, "%s", str);
}

void ShowIconString(u16* icon, int w, int h, const char *format, ...)
{
    char str[STRBUF_SIZE];
    va_list va;
    va_start(va, format);
    vsnprintf(str, STRBUF_SIZE, format, va);
    va_end(va);

    ShowIconStringF(MAIN_SCREEN, icon, w, h, "%s", str);
}

bool ShowPrompt(bool ask, const char *format, ...)
{
    bool ret = true;

    char str[STRBUF_SIZE];
    va_list va;
    va_start(va, format);
    vsnprintf(str, STRBUF_SIZE, format, va);
    va_end(va);

    ClearScreenF(true, false, COLOR_STD_BG);
    DrawStringCenter(MAIN_SCREEN, COLOR_STD_FONT, COLOR_STD_BG, "%s\n \n%s", str,
        (ask) ? STR_A_YES_B_NO : STR_A_TO_CONTINUE);

    while (true) {
        u32 pad_state = InputWait(0);
        if (pad_state & BUTTON_A) break;
        else if (pad_state & BUTTON_B) {
            ret = false;
            break;
        }
    }

    ClearScreenF(true, false, COLOR_STD_BG);

    return ret;
}

#ifndef AUTO_UNLOCK
#define PRNG (*(volatile u32*)0x10011000)
bool ShowUnlockSequence(u32 seqlvl, const char *format, ...) {
    static const int seqcolors[7] = { COLOR_STD_FONT, COLOR_BRIGHTGREEN,
        COLOR_BRIGHTYELLOW, COLOR_ORANGE, COLOR_BRIGHTBLUE, COLOR_BRIGHTBLUE, COLOR_DARKRED };
    const u32 seqlen_max = 7;
    const u32 seqlen = seqlen_max - ((seqlvl < 3) ? 2 : (seqlvl < 4) ? 1 : 0);

    u32 color_bg = COLOR_STD_BG;
    u32 color_font = COLOR_STD_FONT;
    u32 color_off = COLOR_GREY;
    u32 color_on = seqcolors[seqlvl];
    u32 lvl = 0;

    u32 str_width, str_height;
    u32 x, y;

    char str[STRBUF_SIZE];
    va_list va;
    va_start(va, format);
    vsnprintf(str, STRBUF_SIZE, format, va);
    va_end(va);

    str_width = GetDrawStringWidth(str);
    str_height = GetDrawStringHeight(str) + (4*line_height);
    if (str_width < 24 * font_width) str_width = 24 * font_width;
    x = (str_width >= SCREEN_WIDTH_MAIN) ? 0 : (SCREEN_WIDTH_MAIN - str_width) / 2;
    y = (str_height >= SCREEN_HEIGHT) ? 0 : (SCREEN_HEIGHT - str_height) / 2;

    if (seqlvl >= 5) { // special handling
        color_bg = seqcolors[seqlvl];
        color_font = COLOR_BLACK;
        color_off = COLOR_BLACK;
        color_on = COLOR_DARKGREY;
    }

    ClearScreenF(true, false, color_bg);
    DrawStringF(MAIN_SCREEN, x, y, color_font, color_bg, "%s", str);
    #ifndef TIMER_UNLOCK
    DrawStringF(MAIN_SCREEN, x, y + str_height - 28, color_font, color_bg, "%s", STR_TO_PROCEED_ENTER_THIS);

    // generate sequence
    const char *dpad_symbols[] = { "→", "←", "↑", "↓" }; // R L U D

    u32 sequence[seqlen_max];
    const char *seqsymbols[seqlen_max];
    u32 lastlsh = (u32) -1;
    for (u32 n = 0; n < (seqlen-1); n++) {
        u32 lsh = lastlsh;
        while (lsh == lastlsh) lsh = (PRNG & 0x3);
        lastlsh = lsh;
        sequence[n] = BUTTON_RIGHT << lsh;
        seqsymbols[n] = dpad_symbols[lsh];
    }
    sequence[seqlen-1] = BUTTON_A;
    seqsymbols[seqlen-1] = "A";


    while (true) {
        for (u32 n = 0; n < seqlen; n++) {
            DrawStringF(MAIN_SCREEN, x + (n*4*FONT_WIDTH_EXT), y + str_height - 28 + line_height,
                (lvl > n) ? color_on : color_off, color_bg, "<%s>", seqsymbols[n]);
        }
        if (lvl == seqlen)
            break;
        u32 pad_state = InputWait(0);
        if (!(pad_state & BUTTON_ANY))
            continue;
        else if ((pad_state & BUTTON_ANY) == sequence[lvl])
            lvl++;
        else if (pad_state & BUTTON_B)
            break;
        else if (lvl == 0 || !(pad_state & sequence[lvl-1]))
            lvl = 0;
    }
    #else
    DrawStringF(MAIN_SCREEN, x, y + str_height - 28, color_font, color_bg, STR_TO_PROCEED_HOLD_X);

    while (!CheckButton(BUTTON_B)) {
        for (u32 n = 0; n < seqlen; n++) {
            DrawStringF(MAIN_SCREEN, x + (n*4*FONT_WIDTH_EXT), y + str_height - 18,
                (lvl > n) ? color_on : color_off, color_bg, "<%c>", (lvl > n) ? 'X' : ' ');
        }
        if (lvl == seqlen)
            break;

        u32 prev_lvl = lvl;
        while (lvl == prev_lvl) {
            u64 timer = timer_start();
            while ((lvl != 0) && CheckButton(BUTTON_X) && (timer_msec(timer) < 400));

            if (CheckButton(BUTTON_B)) break;
            else if (CheckButton(BUTTON_X)) lvl++;
            else lvl = 0;
        }
    }
    #endif

    ClearScreenF(true, false, COLOR_STD_BG);
    return (lvl >= seqlen);
}
#endif

u32 ShowSelectPrompt(int n, const char** options, const char *format, ...) {
    u32 str_width, str_height;
    u32 x, y, yopt;
    int sel = 0, scroll = 0;
    int n_show = min(n, 10);

    char str[STRBUF_SIZE];
    va_list va;
    va_start(va, format);
    vsnprintf(str, STRBUF_SIZE, format, va);
    va_end(va);

    if (n == 0) return 0; // check for low number of options
    // else if (n == 1) return ShowPrompt(true, "%s\n%s?", str, options[0]) ? 1 : 0;

    str_width = GetDrawStringWidth(str);
    str_height = GetDrawStringHeight(str) + (n_show * (line_height + 2)) + (3 * line_height);
    if (str_width < 24 * font_width) str_width = 24 * font_width;
    for (int i = 0; i < n; i++) if (str_width < GetDrawStringWidth(options[i]) + (3 * font_width))
        str_width = GetDrawStringWidth(options[i]) + (3 * font_width);
    x = (str_width >= SCREEN_WIDTH_MAIN) ? 0 : (SCREEN_WIDTH_MAIN - str_width) / 2;
    y = (str_height >= SCREEN_HEIGHT) ? 0 : (SCREEN_HEIGHT - str_height) / 2;
    yopt = y + GetDrawStringHeight(str) + 8;

    ClearScreenF(true, false, COLOR_STD_BG);
    DrawStringF(MAIN_SCREEN, x, y, COLOR_STD_FONT, COLOR_STD_BG, "%s", str);
    DrawStringF(MAIN_SCREEN, x, yopt + (n_show*(line_height+2)) + line_height, COLOR_STD_FONT, COLOR_STD_BG, "%s", STR_A_SELECT_B_CANCEL);
    while (true) {
        for (int i = scroll; i < scroll+n_show; i++) {
            DrawStringF(MAIN_SCREEN, x, yopt + ((line_height+2)*(i-scroll)), (sel == i) ? COLOR_STD_FONT : COLOR_LIGHTGREY, COLOR_STD_BG, "%2.2s %s",
                (sel == i) ? "->" : "", options[i]);
        }

        // show [n more]
        if (n - n_show - scroll > 0) {
            char more_str[UTF_BUFFER_BYTESIZE(str_width / font_width)], temp_str[UTF_BUFFER_BYTESIZE(64)];
            snprintf(temp_str, sizeof(temp_str), STR_N_MORE, (n - (n_show-1) - scroll));
            ResizeString(more_str, temp_str, str_width / font_width, 8, false);
            DrawString(MAIN_SCREEN, more_str, x, yopt + (line_height+2)*(n_show-1), COLOR_LIGHTGREY, COLOR_STD_BG);
        }
        // show scroll bar
        u32 bar_x = x + str_width + 2;
        const u32 flist_height = (n_show * (line_height + 2));
        const u32 bar_width = 2;
        if (n > n_show) { // draw position bar at the right
            const u32 bar_height_min = 32;
            u32 bar_height = (n_show * flist_height) / n;
            if (bar_height < bar_height_min) bar_height = bar_height_min;
            const u32 bar_y = ((u64) scroll * (flist_height - bar_height)) / (n - n_show) + yopt;

            DrawRectangle(MAIN_SCREEN, bar_x, bar_y, bar_width, bar_height, COLOR_SIDE_BAR);
        }

        u32 pad_state = InputWait(0);
        if (pad_state & BUTTON_DOWN) sel = (sel+1) % n;
        else if (pad_state & BUTTON_UP) sel = (sel+n-1) % n;
        else if (pad_state & BUTTON_RIGHT) sel += n_show;
        else if (pad_state & BUTTON_LEFT) sel -= n_show;
        else if (pad_state & BUTTON_A) break;
        else if (pad_state & BUTTON_B) {
            sel = n;
            break;
        }
        if (sel < 0) sel = 0;
        else if (sel >= n) sel = n-1;

        int prev_scroll = scroll;
        if (sel < scroll) scroll = sel;
        else if (sel ==  n-1 && sel >= (scroll + n_show - 1)) scroll = sel - n_show + 1;
        else if (sel >= (scroll + (n_show-1) - 1)) scroll = sel - (n_show-1) + 1;

        if (scroll != prev_scroll) {
            DrawRectangle(MAIN_SCREEN, x + font_width * 3, yopt, str_width + 4, (n_show * (line_height + 2)), COLOR_STD_BG);
        }
    }

    ClearScreenF(true, false, COLOR_STD_BG);

    return (sel >= n) ? 0 : sel + 1;
}

u32 ShowFileScrollPrompt(int n, const DirEntry** options, bool hide_ext, const char *format, ...) {
    u32 str_height, fname_len;
    u32 x, y, yopt;
    const u32 item_width = SCREEN_WIDTH(MAIN_SCREEN) - 40;
    int sel = 0, scroll = 0;
    int n_show = min(n, 10);

    char str[STRBUF_SIZE];
    va_list va;
    va_start(va, format);
    vsnprintf(str, STRBUF_SIZE, format, va);
    va_end(va);

    if (n == 0) return 0; // check for low number of options
    // else if (n == 1) return ShowPrompt(true, "%s\n%s?", str, options[0]) ? 1 : 0;

    str_height = GetDrawStringHeight(str) + (n_show * (line_height + 2)) + (4 * line_height);
    x = (SCREEN_WIDTH_MAIN - item_width) / 2;
    y = (str_height >= SCREEN_HEIGHT) ? 0 : (SCREEN_HEIGHT - str_height) / 2;
    yopt = y + GetDrawStringHeight(str) + 8;
    fname_len = min(64, item_width / FONT_WIDTH_EXT - 14);

    ClearScreenF(true, false, COLOR_STD_BG);
    DrawStringF(MAIN_SCREEN, x, y, COLOR_STD_FONT, COLOR_STD_BG, "%s", str);
    DrawStringF(MAIN_SCREEN, x, yopt + (n_show*(line_height+2)) + line_height, COLOR_STD_FONT, COLOR_STD_BG, "%s", STR_A_SELECT_B_CANCEL);
    while (true) {
        for (int i = scroll; i < scroll+n_show; i++) {
            char bytestr[16];
            FormatBytes(bytestr, options[i]->size);

            char content_str[UTF_BUFFER_BYTESIZE(fname_len)];
            char temp_str[256];
            strncpy(temp_str, options[i]->name, 256);

            char* dot = strrchr(temp_str, '.');
            if (hide_ext && dot) *dot = '\0';

            ResizeString(content_str, temp_str, fname_len, 8, false);

            DrawStringF(MAIN_SCREEN, x, yopt + ((line_height+2)*(i-scroll)),
                (sel == i) ? COLOR_STD_FONT : COLOR_ENTRY(options[i]), COLOR_STD_BG, "%2.2s %s",
                (sel == i) ? "->" : "", content_str);

            DrawStringF(MAIN_SCREEN, x + item_width - font_width * 11, yopt + ((line_height+2)*(i-scroll)),
                (sel == i) ? COLOR_STD_FONT : COLOR_ENTRY(options[i]), COLOR_STD_BG, "%10.10s",
                (options[i]->type == T_DIR) ? STR_DIR : (options[i]->type == T_DOTDOT) ? "(..)" : bytestr);
        }
        // show [n more]
        if (n - n_show - scroll > 0) {
            char more_str[UTF_BUFFER_BYTESIZE(item_width / font_width)], temp_str[UTF_BUFFER_BYTESIZE(64)];
            snprintf(temp_str, sizeof(temp_str), STR_N_MORE, (n - (n_show-1) - scroll));
            ResizeString(more_str, temp_str, item_width / font_width, 8, false);
            DrawString(MAIN_SCREEN, more_str, x, yopt + (line_height+2)*(n_show-1), COLOR_LIGHTGREY, COLOR_STD_BG);
        }
        // show scroll bar
        u32 bar_x = x + item_width + 2;
        const u32 flist_height = (n_show * (line_height + 2));
        const u32 bar_width = 2;
        if (n > n_show) { // draw position bar at the right
            const u32 bar_height_min = 32;
            u32 bar_height = (n_show * flist_height) / n;
            if (bar_height < bar_height_min) bar_height = bar_height_min;
            const u32 bar_y = ((u64) scroll * (flist_height - bar_height)) / (n - n_show) + yopt;

            DrawRectangle(MAIN_SCREEN, bar_x, yopt, bar_width, (bar_y - yopt), COLOR_STD_BG);
            DrawRectangle(MAIN_SCREEN, bar_x, bar_y + bar_height, bar_width, SCREEN_HEIGHT - (bar_y + bar_height), COLOR_STD_BG);
            DrawRectangle(MAIN_SCREEN, bar_x, bar_y, bar_width, bar_height, COLOR_SIDE_BAR);
        } else DrawRectangle(MAIN_SCREEN, bar_x, yopt, bar_width, flist_height, COLOR_STD_BG);

        u32 pad_state = InputWait(0);
        if (pad_state & BUTTON_DOWN) sel = (sel+1) % n;
        else if (pad_state & BUTTON_UP) sel = (sel+n-1) % n;
        else if (pad_state & BUTTON_RIGHT) sel += n_show;
        else if (pad_state & BUTTON_LEFT) sel -= n_show;
        else if (pad_state & BUTTON_A) break;
        else if (pad_state & BUTTON_B) {
            sel = n;
            break;
        }
        if (sel < 0) sel = 0;
        else if (sel >= n) sel = n-1;
        if (sel < scroll) scroll = sel;
        else if (sel ==  n-1 && sel >= (scroll + n_show - 1)) scroll = sel - n_show + 1;
        else if (sel >= (scroll + (n_show-1) - 1)) scroll = sel - (n_show-1) + 1;
    }

    ClearScreenF(true, false, COLOR_STD_BG);

    return (sel >= n) ? 0 : sel + 1;
}

u32 ShowHotkeyPrompt(u32 n, const char** options, const u32* keys, const char *format, ...) {
    char str[STRBUF_SIZE];
    char* ptr = str;
    va_list va;
    va_start(va, format);
    ptr += vsnprintf(ptr, STRBUF_SIZE, format, va);
    va_end(va);

    ptr += snprintf(ptr, STRBUF_SIZE - (ptr-str), "\n ");
    for (u32 i = 0; i < n; i++) {
        char buttonstr[16];
        ButtonToString(keys[i], buttonstr);
        ptr += snprintf(ptr, STRBUF_SIZE - (ptr-str), "\n<%s> %s", buttonstr, options[i]);
    }
    ptr += snprintf(ptr, STRBUF_SIZE - (ptr-str), "\n \n<%s> %s", "B", STR_CANCEL);

    ClearScreenF(true, false, COLOR_STD_BG);
    DrawStringCenter(MAIN_SCREEN, COLOR_STD_FONT, COLOR_STD_BG, "%s", str);

    u32 sel = 0;
    while (!sel) {
        u32 pad_state = InputWait(0);
        if (pad_state & BUTTON_B) break;
        for (u32 i = 0; i < n; i++) {
            if (keys[i] & pad_state) {
                sel = i+1;
                break;
            }
        }
    }

    ClearScreenF(true, false, COLOR_STD_BG);
    return sel;
}

bool ShowInputPrompt(char* inputstr, u32 max_size, u32 resize, const char* alphabet, const char *format, va_list va) {
    const u32 alphabet_size = strnlen(alphabet, 256);
    const u32 input_shown_length = 22;
    const u32 fast_scroll = 4;
    const u64 aprv_delay = 128;

    u32 str_width, str_height;
    u32 x, y;

    char str[STRBUF_SIZE];
    vsnprintf(str, STRBUF_SIZE, format, va);

    // check / fix up the input string if required
    if (max_size < 2) return false; // catching this, too
    if ((*inputstr == '\0') || (resize && (strnlen(inputstr, max_size - 1) % resize))) {
        memset(inputstr, alphabet[0], resize); // set the string if it is not set or invalid
        inputstr[resize] = '\0';
    }

    str_width = GetDrawStringWidth(str);
    str_height = GetDrawStringHeight(str) + 88;
    if (str_width < (24 * font_width)) str_width = 24 * font_width;
    x = (str_width >= SCREEN_WIDTH_MAIN) ? 0 : (SCREEN_WIDTH_MAIN - str_width) / 2;
    y = (str_height >= SCREEN_HEIGHT) ? 0 : (SCREEN_HEIGHT - str_height) / 2;

    ClearScreenF(true, false, COLOR_STD_BG);
    DrawStringF(MAIN_SCREEN, x, y, COLOR_STD_FONT, COLOR_STD_BG, "%s", str);
    DrawStringF(MAIN_SCREEN, x + 8, y + str_height - 40, COLOR_STD_FONT, COLOR_STD_BG,
        "%s\n%s", STR_R_FAST_SCROLL_L_CLEAR_DATA, resize ? STR_X_REMOVE_CHAR_Y_INSERT_CHAR : "");

    // wait for all keys released
    while (HID_ReadState() & BUTTON_ANY);

    int cursor_a = -1;
    u32 cursor_s = 0;
    u32 scroll = 0;
    u64 aprv = 0;
    bool ret = false;

    while (true) {
        u32 inputstr_size = strnlen(inputstr, max_size - 1);

        if (cursor_s < scroll) {
            scroll = cursor_s;
        } else {
            int scroll_adjust = -input_shown_length;
            for (u32 i = scroll; i < cursor_s; i++) {
                if (i >= inputstr_size || (inputstr[i] & 0xC0) != 0x80) scroll_adjust++;
            }

            for (int i = 0; i <= scroll_adjust; i++)
                scroll += scroll >= inputstr_size ? 1 : GetCharSize(inputstr + scroll);
        }

        u32 input_shown_size = 0;
        for (u32 i = 0; i < input_shown_length || (scroll + input_shown_size < inputstr_size && (inputstr[scroll + input_shown_size] & 0xC0) == 0x80); input_shown_size++) {
            if (scroll + input_shown_size >= inputstr_size || (inputstr[scroll + input_shown_size] & 0xC0) != 0x80) i++;
        }

        u16 cpos = 0;
        for (u16 i = scroll; i < cursor_s; i++) {
            if (i >= inputstr_size || (inputstr[i] & 0xC0) != 0x80) cpos++;
        }

        DrawStringF(MAIN_SCREEN, x, y + str_height - 76, COLOR_STD_FONT, COLOR_STD_BG, "%c%-*.*s%c\n%-*.*s^%-*.*s",
            (scroll) ? '<' : '|',
            (int) input_shown_size,
            (int) input_shown_size,
            (scroll > inputstr_size) ? "" : inputstr + scroll,
            (inputstr_size - (s32) scroll > input_shown_size) ? '>' : '|',
            (int) 1 + cpos,
            (int) 1 + cpos,
            "",
            (int) input_shown_length - cpos,
            (int) input_shown_length - cpos,
            ""
        );

        if (cursor_a < 0) {
            for (cursor_a = alphabet_size - 1; (cursor_a > 0) && (alphabet[cursor_a] != inputstr[cursor_s]); cursor_a--);
        }

        // alphabet preview
        if (alphabet_size > (SCREEN_WIDTH(MAIN_SCREEN) / font_width)) {
            const u32 aprv_y = y + str_height - 60;
            if (timer_msec(aprv) < aprv_delay) {
                const u32 aprv_pad = 1;
                const u32 aprv_cx = x + ((1 + cursor_s - scroll) * font_width);
                u32 aprv_x = aprv_cx % (font_width + aprv_pad);
                u32 aprv_n = ((SCREEN_WIDTH(MAIN_SCREEN) - aprv_x) / (font_width + aprv_pad)) - 1;
                int aprv_a = cursor_a - ((aprv_cx - aprv_x) / (font_width + aprv_pad));
                while (aprv_a < 0) aprv_a += alphabet_size;
                for (u32 i = 0; i < aprv_n; i++) {
                    DrawCharacter(MAIN_SCREEN, alphabet[aprv_a], aprv_x, aprv_y,
                        (aprv_a == cursor_a) ? COLOR_WHITE : COLOR_GREY, COLOR_STD_BG);
                    if (++aprv_a >= (int) alphabet_size) aprv_a -= alphabet_size;
                    aprv_x += font_width + aprv_pad;
                }
            } else DrawRectangle(MAIN_SCREEN, 0, aprv_y, SCREEN_WIDTH(MAIN_SCREEN), font_height, COLOR_STD_BG);
        }

        u32 pad_state = InputWait(3);
        if (pad_state & (BUTTON_UP|BUTTON_DOWN|BUTTON_R1)) aprv = timer_start();
        if (pad_state & BUTTON_A) {
            ret = true;
            break;
        } else if (pad_state & BUTTON_B) {
            break;
        } else if (pad_state & BUTTON_L1) {
            cursor_a = 0;
            memset(inputstr, alphabet[0], inputstr_size);
            if (resize) {
                cursor_s = 0;
                inputstr[resize] = '\0';
            }
        } else if (pad_state & BUTTON_X) {
            if (resize && (inputstr_size > resize)) {
                u32 char_index = 0;
                for(u32 i = 0; i < cursor_s; i++) {
                    if (i >= inputstr_size || (inputstr[i] & 0xC0) != 0x80) char_index++;
                }

                u32 to_index = char_index - (char_index % resize);
                u32 from_index = to_index + resize;

                char* inputto = inputstr + cursor_s;
                for (u32 i = 0; i < char_index - to_index; i++) {
                    inputto -= GetPrevCharSize(inputto);
                }
                char* inputfrom = inputto;
                for (u32 i = 0; i < from_index - to_index; i++) {
                    inputfrom += GetCharSize(inputfrom);
                }

                memmove(inputto, inputfrom, max_size - (inputfrom - inputstr));
                inputstr_size -= inputfrom - inputto;
                while (cursor_s >= inputstr_size || (inputstr[cursor_s] & 0xC0) == 0x80)
                    cursor_s--;
                cursor_a = -1;
            } else if (resize == 1) {
                inputstr[0] = alphabet[0];
                cursor_a = 0;
            }
        } else if (pad_state & BUTTON_Y) {
            if (resize && (inputstr_size < max_size - resize)) {
                char* inputfrom = inputstr + cursor_s - (cursor_s % resize);
                char* inputto = inputstr + cursor_s - (cursor_s % resize) + resize;
                memmove(inputto, inputfrom, max_size - (inputto - inputstr));
                inputstr_size += resize;
                memset(inputfrom, alphabet[0], resize);
                cursor_a = 0;
            }
        } else if (pad_state & BUTTON_UP) {
            int size = GetCharSize(inputstr + cursor_s);
            if(size > 1) {
                memmove(inputstr + cursor_s, inputstr + cursor_s + size - 1, inputstr_size - cursor_s + size - 1);
            }

            cursor_a += (pad_state & BUTTON_R1) ? fast_scroll : 1;
            cursor_a = cursor_a % alphabet_size;
            inputstr[cursor_s] = alphabet[cursor_a];
        } else if (pad_state & BUTTON_DOWN) {
            int size = GetCharSize(inputstr + cursor_s);
            if(size > 1) {
                memmove(inputstr + cursor_s, inputstr + cursor_s + size - 1, inputstr_size - cursor_s + size - 1);
            }

            cursor_a -= (pad_state & BUTTON_R1) ? fast_scroll : 1;
            if (cursor_a < 0) cursor_a = alphabet_size + cursor_a;
            inputstr[cursor_s] = alphabet[cursor_a];
        } else if (pad_state & BUTTON_LEFT) {
            if (cursor_s > 0) cursor_s -= GetPrevCharSize(inputstr + cursor_s);
            cursor_a = -1;
        } else if (pad_state & BUTTON_RIGHT) {
            int size = cursor_s > inputstr_size ? 1 : GetCharSize(inputstr + cursor_s);
            if (cursor_s + size < max_size - 1) cursor_s += size;
            if (cursor_s >= inputstr_size) {
                memset(inputstr + cursor_s, alphabet[0], resize);
                inputstr[cursor_s + resize] = '\0';
            }
            cursor_a = -1;
        }
    }
    // remove any trailing spaces
    for (char* cc = inputstr + strnlen(inputstr, max_size) - 1;
        (*cc == ' ') && (cc > inputstr); *(cc--) = '\0');

    ClearScreenF(true, false, COLOR_STD_BG);

    return ret;
}

bool ShowStringPrompt(char* inputstr, u32 max_size, const char *format, ...) {
    const char* alphabet = " aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ(){}[]'`^,~*?!@#$%&0123456789=+-_.";
    bool ret = false;
    va_list va;

    va_start(va, format);
    ret = ShowInputPrompt(inputstr, max_size, 1, alphabet, format, va);
    va_end(va);

    return ret;
}

u64 ShowHexPrompt(u64 start_val, u32 n_digits, const char *format, ...) {
    const char* alphabet = "0123456789ABCDEF";
    char inputstr[16 + 1];
    u64 ret = 0;
    va_list va;

    if (n_digits > 16) n_digits = 16;
    snprintf(inputstr, sizeof(inputstr), "%0*llX", (int) n_digits, start_val);

    va_start(va, format);
    if (ShowInputPrompt(inputstr, n_digits + 1, 0, alphabet, format, va)) {
        sscanf(inputstr, "%llX", &ret);
    } else ret = (u64) -1;
    va_end(va);

    return ret;
}

u64 ShowNumberPrompt(u64 start_val, const char *format, ...) {
    const char* alphabet = "0123456789";
    char inputstr[20 + 1];
    u64 ret = 0;
    va_list va;

    snprintf(inputstr, sizeof(inputstr), "%llu", start_val);

    va_start(va, format);
    if (ShowInputPrompt(inputstr, 20 + 1, 1, alphabet, format, va)) {
        sscanf(inputstr, "%llu", &ret);
    } else ret = (u64) -1;
    va_end(va);

    return ret;
}

bool ShowDataPrompt(u8* data, u32* size, const char *format, ...) {
    const char* alphabet = "0123456789ABCDEF";
    char inputstr[128 + 1]; // maximum size of data: 64 byte
    bool ret = false;
    va_list va;

    if (*size == 0) *inputstr = 0;
    else if (*size > 64) *size = 64;

    for (u32 i = 0; i < *size; i++)
        snprintf(inputstr + (2*i), 128 + 1 - (2*i), "%02X", (unsigned int) data[i]);

    va_start(va, format);
    if (ShowInputPrompt(inputstr, 128 + 1, 2, alphabet, format, va)) {
        *size = strnlen(inputstr, 128) / 2;
        for (u32 i = 0; i < *size; i++) {
            char bytestr[2 + 1];
            unsigned int byte;
            strncpy(bytestr, inputstr + (2*i), 2);
            sscanf(bytestr, "%02X", &byte);
            data[i] = (u8) byte;
        }
        ret = true;
    }
    va_end(va);

    return ret;
}


bool ShowRtcSetterPrompt(void* time, const char *format, ...) {
    DsTime* dstime = (DsTime*) time;
    u32 str_width, str_height;
    u32 x, y;

    char str[STRBUF_SIZE];
    va_list va;
    va_start(va, format);
    vsnprintf(str, STRBUF_SIZE, format, va);
    va_end(va);

    // check the initial time
    if (!is_valid_dstime(dstime)) {
        dstime->bcd_h = dstime->bcd_m = dstime->bcd_s = 0x00;
        dstime->bcd_D = dstime->bcd_M = 0x01;
        dstime->bcd_Y = 0x00;
    }

    str_width = GetDrawStringWidth(str);
    str_height = GetDrawStringHeight(str) + (4*line_height);
    if (str_width < (19 * font_width)) str_width = 19 * font_width;
    x = (str_width >= SCREEN_WIDTH_MAIN) ? 0 : (SCREEN_WIDTH_MAIN - str_width) / 2;
    y = (str_height >= SCREEN_HEIGHT) ? 0 : (SCREEN_HEIGHT - str_height) / 2;

    ClearScreenF(true, false, COLOR_STD_BG);
    DrawStringF(MAIN_SCREEN, x, y, COLOR_STD_FONT, COLOR_STD_BG, "%s", str);

    int cursor = 0;
    bool ret = false;
    while (true) {
        static const int val_max[] = { 99, 12, 31, 23, 59, 59 };
        static const int val_min[] = {  0,  1,  1,  0,  0,  0 };
        u8* bcd = &(((u8*)dstime)[(cursor<3) ? (6-cursor) : (6-1-cursor)]);
        int val = BCD2NUM(*bcd);
        int max = val_max[cursor];
        int min = val_min[cursor];
        DrawStringF(MAIN_SCREEN, x, y + str_height - 28, COLOR_STD_FONT, COLOR_STD_BG, "YYYY-MM-DD hh:mm:ss");
        DrawStringF(MAIN_SCREEN, x, y + str_height - 18, COLOR_STD_FONT, COLOR_STD_BG, "20%02lX-%02lX-%02lX %02lX:%02lX:%02lX\n  %-*.*s^^%-*.*s",
            (u32) dstime->bcd_Y, (u32) dstime->bcd_M, (u32) dstime->bcd_D, (u32) dstime->bcd_h, (u32) dstime->bcd_m, (u32) dstime->bcd_s,
            cursor * 3, cursor * 3, "", 17 - 2 - (cursor * 3), 17 - 2 - (cursor * 3), "");

        // user input
        u32 pad_state = InputWait(0);
        if ((pad_state & BUTTON_A) && is_valid_dstime(dstime)) {
            ret = true;
            break;
        } else if (pad_state & BUTTON_B) {
            break;
        } else if (pad_state & BUTTON_UP) {
            val += (pad_state & BUTTON_R1) ? 10 : 1;
            if (val > max) val = max;
        } else if (pad_state & BUTTON_DOWN) {
            val -= (pad_state & BUTTON_R1) ? 10 : 1;
            if (val < min) val = min;
        } else if (pad_state & BUTTON_LEFT) {
            if (--cursor < 0) cursor = 5;
        } else if (pad_state & BUTTON_RIGHT) {
            if (++cursor > 5) cursor = 0;
        }

        // update bcd
        *bcd = NUM2BCD(val);
    }

    ClearScreenF(true, false, COLOR_STD_BG);
    return ret;
}

static bool ShowProgressCancelCheck(u64 msec)
{
    static u64 last_msec = 0;

    if (msec != 0 && CheckButton(BUTTON_B)) return (msec > last_msec + PROGRESS_CANCEL_MIN_HOLD_MS);

    last_msec = msec;
    return false;
}

bool ShowProgress(u64 current, u64 total, const char* opstr)
{
    static u32 last_prog_width = 0;
    static u64 timer = 0;
    const u32 bar_width = 240;
    const u32 bar_height = 12;
    const u32 bar_pos_x = (SCREEN_WIDTH_MAIN - bar_width) / 2;
    const u32 bar_pos_y = (SCREEN_HEIGHT / 2) - bar_height - 2 - 10;
    const u32 text_pos_y = bar_pos_y + bar_height + 2;
    u32 prog_width = ((total > 0) && (current <= total)) ? (current * (bar_width-4)) / total : 0;
    u32 prog_percent = ((total > 0) && (current <= total)) ? (current * 100) / total : 0;
    char tempstr[UTF_BUFFER_BYTESIZE(64)];
    char progstr[UTF_BUFFER_BYTESIZE(64)];

    static u64 last_msec_elapsed = 0;
    static u64 last_sec_remain = 0;
    if (!current) {
        timer = timer_start();
        last_sec_remain = 0;
        ShowProgressCancelCheck(0); // reset counter
    } else if (timer_msec(timer) < last_msec_elapsed + PROGRESS_REFRESH_RATE) return !ShowProgressCancelCheck(timer_msec(timer));
    last_msec_elapsed = timer_msec(timer);
    u64 sec_elapsed = (total > 0) ? timer_sec( timer ) : 0;
    u64 sec_total = (current > 0) ? (sec_elapsed * total) / current : 0;
    u64 sec_remain = (!last_sec_remain) ? (sec_total - sec_elapsed) : ((last_sec_remain + (sec_total - sec_elapsed) + 1) / 2);
    if (sec_remain >= 60 * 60) sec_remain = 60 * 60 - 1;
    last_sec_remain = sec_remain;

    if (!current || last_prog_width > prog_width) {
        ClearScreenF(true, false, COLOR_STD_BG);
        DrawRectangle(MAIN_SCREEN, bar_pos_x, bar_pos_y, bar_width, bar_height, COLOR_STD_FONT);
        DrawRectangle(MAIN_SCREEN, bar_pos_x + 1, bar_pos_y + 1, bar_width - 2, bar_height - 2, COLOR_STD_BG);
    }
    DrawRectangle(MAIN_SCREEN, bar_pos_x + 2, bar_pos_y + 2, prog_width, bar_height - 4, COLOR_STD_FONT);
    DrawRectangle(MAIN_SCREEN, bar_pos_x + 2 + prog_width, bar_pos_y + 2, (bar_width-4) - prog_width, bar_height - 4, COLOR_STD_BG);

    TruncateString(progstr, opstr, min(63, (bar_width / FONT_WIDTH_EXT) - 7), 8);
    snprintf(tempstr, sizeof(tempstr), "%s (%lu%%)", progstr, prog_percent);
    ResizeString(progstr, tempstr, bar_width / FONT_WIDTH_EXT, 8, false);
    DrawString(MAIN_SCREEN, progstr, bar_pos_x, text_pos_y, COLOR_STD_FONT, COLOR_STD_BG);
    if (sec_elapsed >= 1) {
        snprintf(tempstr, sizeof(tempstr), STR_ETA_N_MIN_N_SEC, sec_remain / 60, sec_remain % 60);
        ResizeString(progstr, tempstr, 16, 8, true);
        DrawString(MAIN_SCREEN, progstr, bar_pos_x + bar_width - 1 - (FONT_WIDTH_EXT * 16),
            bar_pos_y - line_height - 1, COLOR_STD_FONT, COLOR_STD_BG);
    }
    DrawString(MAIN_SCREEN, STR_HOLD_B_TO_CANCEL, bar_pos_x + 2, text_pos_y + 14, COLOR_STD_FONT, COLOR_STD_BG);

    last_prog_width = prog_width;

    return !ShowProgressCancelCheck(last_msec_elapsed);
}

int ShowBrightnessConfig(int set_brightness)
{
    const int old_brightness = set_brightness;
    u32 btn_input, bar_count;
    int bar_x_pos, bar_y_pos, bar_width, bar_height;

    const char *brightness_str = STR_BRIGHTNESS_CONTROLS;
    static const u16 brightness_slider_colmasks[] = {
        COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_WHITE
    };

    ClearScreen(MAIN_SCREEN, COLOR_STD_BG);

    bar_count = countof(brightness_slider_colmasks);
    bar_width = (SCREEN_WIDTH_MAIN / 8) * 6;
    bar_x_pos = SCREEN_WIDTH_MAIN / 8;

    bar_height = 10;
    bar_y_pos = SCREEN_HEIGHT / 4;

    // default to average brightness if invalid / automatic
    if (set_brightness < BRIGHTNESS_MIN || set_brightness > BRIGHTNESS_MAX)
        set_brightness = (BRIGHTNESS_MAX + BRIGHTNESS_MIN) / 2;

    // draw initial UI stuff
    DrawStringF(MAIN_SCREEN,
        (SCREEN_WIDTH_MAIN - GetDrawStringWidth(brightness_str)) / 2,
        (SCREEN_HEIGHT / 4) * 2, COLOR_STD_FONT, COLOR_STD_BG, "%s", brightness_str);

    // draw all color gradient bars
    for (int x = 0; x < bar_width; x++) {
        u32 intensity;
        u16 intensity_mask;

        intensity = FIXP_TO_INT(fixp_changespace(
            INT_TO_FIXP(x),
            INT_TO_FIXP(0), INT_TO_FIXP(bar_width),
            INT_TO_FIXP(0), INT_TO_FIXP(256)
        ));

        intensity_mask = RGB(intensity, intensity, intensity);

        for (u32 b = 0; b < bar_count; b++) {
            u16 *screen_base = &MAIN_SCREEN[PIXEL_OFFSET(bar_x_pos + x, (b * bar_height) + bar_y_pos)];
            for (int y = 0; y < bar_height; y++)
                *(screen_base++) = brightness_slider_colmasks[b] & intensity_mask;
        }
    }

    while(1) {
        int prev_brightness, slider_x_pos, slider_y_pos;

        prev_brightness = set_brightness;
        slider_y_pos = bar_y_pos + (bar_height * 3) + font_height;

        if (set_brightness != BRIGHTNESS_AUTOMATIC) {
            slider_x_pos = bar_x_pos + font_width + FIXP_TO_INT(fixp_changespace(
                INT_TO_FIXP(set_brightness),
                INT_TO_FIXP(BRIGHTNESS_MIN), INT_TO_FIXP(BRIGHTNESS_MAX),
                INT_TO_FIXP(0), INT_TO_FIXP(bar_width)
            ));

            // redraw the slider position character (if necessary)
            DrawCharacter(MAIN_SCREEN, '^', slider_x_pos,
                slider_y_pos, COLOR_STD_FONT, COLOR_STD_BG);
        }

        btn_input = InputWait(0);

        // draw a small rectangle to clear the character
        if (set_brightness != BRIGHTNESS_AUTOMATIC) {
            DrawRectangle(MAIN_SCREEN, slider_x_pos,
                slider_y_pos, font_width, font_height, COLOR_STD_BG);
        }

        if (btn_input & BUTTON_LEFT) {
            set_brightness -= 10;
        } else if (btn_input & BUTTON_RIGHT) {
            set_brightness += 10;
        } else if (btn_input & BUTTON_X) {
            set_brightness = BRIGHTNESS_AUTOMATIC;
            break;
        } else if (btn_input & BUTTON_B) {
            set_brightness = old_brightness;
            break;
        } else if (btn_input & BUTTON_A) {
            break;
        }

        if (set_brightness != BRIGHTNESS_AUTOMATIC)
            set_brightness = clamp(set_brightness, BRIGHTNESS_MIN, BRIGHTNESS_MAX);

        if (set_brightness != prev_brightness)
            SetScreenBrightness(set_brightness);
    }

    ClearScreen(MAIN_SCREEN, COLOR_STD_BG);
    SetScreenBrightness(set_brightness);
    return set_brightness;
}
