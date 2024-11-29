// Copyright 2013 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vram.h>
#include "common.h"
#include "colors.h"
#include "fsdir.h" // only required for ShowFileScrollPrompt


#define BYTES_PER_PIXEL 2
#define SCREEN_HEIGHT 240
#define SCREEN_WIDTH(s) ((s == TOP_SCREEN) ? SCREEN_WIDTH_TOP : SCREEN_WIDTH_BOT)
#define SCREEN_WIDTH_TOP 400
#define SCREEN_WIDTH_BOT 320
#define SCREEN_SIZE(s) ((s == TOP_SCREEN) ? SCREEN_SIZE_TOP : SCREEN_SIZE_BOT)
#define SCREEN_SIZE_TOP (SCREEN_WIDTH_TOP * SCREEN_HEIGHT * BYTES_PER_PIXEL)
#define SCREEN_SIZE_BOT (SCREEN_WIDTH_BOT * SCREEN_HEIGHT * BYTES_PER_PIXEL)
#define FONT_WIDTH_EXT   GetFontWidth()
#define FONT_HEIGHT_EXT  GetFontHeight()

#define UTF_MAX_BYTES_PER_RUNE 4
#define UTF_BUFFER_BYTESIZE(rune_count) (((rune_count) + 1) * UTF_MAX_BYTES_PER_RUNE)

#define PRINTF_ARGS(n) __attribute__ ((format (printf, (n), (n) + 1)))

#define TOP_SCREEN          ((u16*)VRAM_TOP_LA)
#define BOT_SCREEN          ((u16*)VRAM_BOT_A)

#ifdef SWITCH_SCREENS
#define MAIN_SCREEN         TOP_SCREEN
#define ALT_SCREEN          BOT_SCREEN
#define SCREEN_WIDTH_MAIN   SCREEN_WIDTH_TOP
#define SCREEN_WIDTH_ALT    SCREEN_WIDTH_BOT
#else
#define MAIN_SCREEN         BOT_SCREEN
#define ALT_SCREEN          TOP_SCREEN
#define SCREEN_WIDTH_MAIN   SCREEN_WIDTH_BOT
#define SCREEN_WIDTH_ALT    SCREEN_WIDTH_TOP
#endif

#define COLOR_TRANSPARENT   COLOR_SUPERFUCHSIA


#ifndef AUTO_UNLOCK
bool PRINTF_ARGS(2) ShowUnlockSequence(u32 seqlvl, const char *format, ...);
#else
#define ShowUnlockSequence ShowPrompt
#endif

const u8* GetFontFromPbm(const void* pbm, const u32 riff_size, u32* w, u32* h);
const u8* GetFontFromRiff(const void* riff, const u32 riff_size, u32* w, u32* h, u16* count);
bool SetFont(const void* font, const u32 font_size);

u16 GetColor(const u16 *screen, int x, int y);

void ClearScreen(u16 *screen, u32 color);
void ClearScreenF(bool clear_main, bool clear_alt, u32 color);
void DrawPixel(u16 *screen, int x, int y, u32 color);
void DrawRectangle(u16 *screen, int x, int y, u32 width, u32 height, u32 color);
void DrawBitmap(u16 *screen, int x, int y, u32 w, u32 h, const u16* bitmap);
void DrawQrCode(u16 *screen, const u8* qrcode);

void DrawCharacter(u16 *screen, u32 character, int x, int y, u32 color, u32 bgcolor);
void DrawString(u16 *screen, const char *str, int x, int y, u32 color, u32 bgcolor);
void PRINTF_ARGS(6) DrawStringF(u16 *screen, int x, int y, u32 color, u32 bgcolor, const char *format, ...);
void PRINTF_ARGS(4) DrawStringCenter(u16 *screen, u32 color, u32 bgcolor, const char *format, ...);

u32 GetCharSize(const char* str);
u32 GetPrevCharSize(const char* str);
const char* GetNextChar(const char* chr);
const char* GetPrevChar(const char* chr);
void IncChar(const char** chr);
void DecChar(const char** chr);

u32 GetDrawStringHeight(const char* str);
u32 GetDrawStringWidth(const char* str);
u32 GetFontWidth(void);
u32 GetFontHeight(void);

void MultiLineString(char* dest, const char* orig, int llen, int maxl);
void WordWrapString(char* str, int llen);
void ResizeString(char* dest, const char* orig, int nlength, int tpos, bool align_right);
void TruncateString(char* dest, const char* orig, int nlength, int tpos);
void FormatNumber(char* str, u64 number);
void FormatBytes(char* str, u64 bytes);

void PRINTF_ARGS(1) ShowString(const char *format, ...);
void PRINTF_ARGS(2) ShowStringF(u16* screen, const char *format, ...);
void PRINTF_ARGS(4) ShowIconString(u16* icon, int w, int h, const char *format, ...);
void PRINTF_ARGS(5) ShowIconStringF(u16* screen, u16* icon, int w, int h, const char *format, ...);
bool PRINTF_ARGS(2) ShowPrompt(bool ask, const char *format, ...);
u32 PRINTF_ARGS(3) ShowSelectPrompt(int n, const char** options, const char *format, ...);
u32 PRINTF_ARGS(4) ShowFileScrollPrompt(int n, const DirEntry** entries, bool hide_ext, const char *format, ...);
u32 PRINTF_ARGS(4) ShowHotkeyPrompt(u32 n, const char** options, const u32* keys, const char *format, ...);
bool PRINTF_ARGS(3) ShowStringPrompt(char* inputstr, u32 max_size, const char *format, ...);
u64 PRINTF_ARGS(3) ShowHexPrompt(u64 start_val, u32 n_digits, const char *format, ...);
u64 PRINTF_ARGS(2) ShowNumberPrompt(u64 start_val, const char *format, ...);
bool PRINTF_ARGS(3) ShowDataPrompt(u8* data, u32* size, const char *format, ...);
bool PRINTF_ARGS(2) ShowRtcSetterPrompt(void* time, const char *format, ...);
bool ShowProgress(u64 current, u64 total, const char* opstr);

int ShowBrightnessConfig(int set_brightness);

static inline u16 rgb888_to_rgb565(u32 rgb) {
	u8 r, g, b;
	r = (rgb >> 16) & 0x1F;
	g = (rgb >> 8) & 0x3F;
	b = (rgb >> 0) & 0x1F;
	return (r << 11) | (g << 5) | b;
}

static inline u16 rgb888_buf_to_rgb565(u8 *rgb) {
	u8 r, g, b;
	r = (rgb[0] >> 3);
	g = (rgb[1] >> 2);
	b = (rgb[2] >> 3);
	return (r << 11) | (g << 5) | b;
}
