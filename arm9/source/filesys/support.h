#pragma once

#include "common.h"

// scripts / payloads dir names
#define LANGUAGES_DIR   "languages"
#define SCRIPTS_DIR     "scripts"
#define LUASCRIPTS_DIR  "luascripts"
#define PAYLOADS_DIR    "payloads"

bool CheckSupportFile(const char* fname);
size_t LoadSupportFile(const char* fname, void* buffer, size_t max_len);
bool SaveSupportFile(const char* fname, void* buffer, size_t len);
bool SetAsSupportFile(const char* fname, const char* source);

bool GetSupportDir(char* path, const char* dname);
bool CheckSupportDir(const char* fpath);
bool FileSelectorSupport(char* result, const char* text, const char* dname, const char* pattern);
