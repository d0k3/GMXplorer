#pragma once
#include "gm9lua.h"
#include "ui.h"
#include "fs.h"
#include "png.h"
#include "swkbd.h"

#define GM9LUA_UILIBNAME "ui"

void ShiftOutputBufferUp(void);
void ClearOutputBuffer(void);
void RenderOutputBuffer(void);
void WriteToOutputBuffer(char* text);
int gm9lua_open_ui(lua_State* L);
