#pragma once

#include "common.h"

#define IMG_FAT     (1UL<<0)
#define IMG_NAND    (1UL<<1)
#define GAME_CIA    (1UL<<2)
#define GAME_NCSD   (1UL<<3)
#define GAME_NCCH   (1UL<<4)
#define GAME_TMD    (1UL<<5)
#define GAME_EXEFS  (1UL<<6)
#define GAME_ROMFS  (1UL<<7)
#define GAME_BOSS   (1UL<<8)
#define GAME_NUSCDN (1UL<<9)
#define GAME_TICKET (1UL<<10)
#define GAME_SMDH   (1UL<<11)
#define GAME_NDS    (1UL<<12)
#define SYS_FIRM    (1UL<<13)
#define SYS_TICKDB  (1UL<<14)
#define BIN_NCCHNFO (1UL<<15)
#define BIN_LAUNCH  (1UL<<16)
#define BIN_SUPPORT (1UL<<17)
#define SYS_MOVABLE (1UL<<18)
#define TYPE_BASE   0x00FFFFFF // 24 bit reserved for base types

#define FLAG_CTR    (1UL<<29)
#define FLAG_NUSCDN (1UL<<30)
#define FLAG_CXI    (1UL<<31)

#define FTYPE_MOUNTABLE(tp)     (tp&(IMG_FAT|IMG_NAND|GAME_CIA|GAME_NCSD|GAME_NCCH|GAME_EXEFS|GAME_ROMFS|SYS_FIRM|SYS_TICKDB))
#define FYTPE_VERIFICABLE(tp)   (tp&(IMG_NAND|GAME_CIA|GAME_NCSD|GAME_NCCH|GAME_TMD|GAME_BOSS|SYS_FIRM))
#define FYTPE_DECRYPTABLE(tp)   (tp&(GAME_CIA|GAME_NCSD|GAME_NCCH|GAME_BOSS|GAME_NUSCDN|SYS_FIRM))
#define FYTPE_ENCRYPTABLE(tp)   (tp&(GAME_CIA|GAME_NCSD|GAME_NCCH|GAME_BOSS))
#define FTYPE_BUILDABLE(tp)     (tp&(GAME_NCSD|GAME_NCCH|GAME_TMD))
#define FTYPE_BUILDABLE_L(tp)   (FTYPE_BUILDABLE(tp) && (tp&(GAME_TMD)))
#define FTYPE_TITLEINFO(tp)     (tp&(GAME_SMDH|GAME_NCCH|GAME_NCSD|GAME_CIA|GAME_TMD|GAME_NDS))
#define FTYPE_TRANSFERABLE(tp)  ((u32) (tp&(IMG_FAT|FLAG_CTR)) == (u32) (IMG_FAT|FLAG_CTR))
#define FTYPE_HSINJECTABLE(tp)  ((u32) (tp&(GAME_NCCH|FLAG_CXI)) == (u32) (GAME_NCCH|FLAG_CXI))
#define FTYPE_RESTORABLE(tp)    (tp&(IMG_NAND))
#define FTYPE_EBACKUP(tp)       (tp&(IMG_NAND))
#define FTYPE_XORPAD(tp)        (tp&(BIN_NCCHNFO))
#define FTYPE_PAYLOAD(tp)       (tp&(BIN_LAUNCH))
#define FTYPE_ID0(tp)           (tp&(IMG_NAND|SYS_MOVABLE))

u32 IdentifyFileType(const char* path);
