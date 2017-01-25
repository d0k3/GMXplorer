#pragma once

#include "common.h"

#define IMG_FAT    (1<<0)
#define IMG_NAND   (1<<1)

#define GAME_CIA   (1<<2)
#define GAME_NCSD  (1<<3)
#define GAME_NCCH  (1<<4)
#define GAME_TMD   (1<<5)
#define GAME_EXEFS (1<<6)
#define GAME_ROMFS (1<<7)

#define SYS_FIRM   (1<<8)

#define MISC_NINFO (1<<9)

#define SYS_LAUNCH (1<<10)

#define FTYPE_MOUNTABLE     (IMG_FAT|IMG_NAND|GAME_CIA|GAME_NCSD|GAME_NCCH|GAME_EXEFS|GAME_ROMFS|SYS_FIRM)
#define FYTPE_VERIFICABLE   (IMG_NAND|GAME_CIA|GAME_NCSD|GAME_NCCH|GAME_TMD|SYS_FIRM)
#define FYTPE_DECRYPTABLE   (GAME_CIA|GAME_NCSD|GAME_NCCH|SYS_FIRM)
#define FTYPE_BUILDABLE     (GAME_NCSD|GAME_NCCH|GAME_TMD)
#define FTYPE_BUILDABLE_L   (FTYPE_BUILDABLE&(GAME_TMD))
#define FTYPE_RESTORABLE    (IMG_NAND)
#define FTYPE_XORPAD        (MISC_NINFO)
#define FTYPE_PAYLOAD       (SYS_LAUNCH)

u32 IdentifyFileType(const char* path);
