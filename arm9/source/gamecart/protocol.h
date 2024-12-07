// Copyright 2014 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once
#include "common.h"

#define REG_CARDCTL     (*(vu16*)0x1000000C)
#define REG_CARDSTATUS  (*(vu8* )0x10000010)
#define REG_CARDCYCLES0 (*(vu16*)0x10000012)
#define REG_CARDCYCLES1 (*(vu16*)0x10000014)


#define LATENCY 0x822Cu
#define BSWAP32(n)  __builtin_bswap32(n)


void Cart_Init(void);
int Cart_IsInserted(void);
u32 Cart_GetID(void);
u32 Cart_GetID2(void);
void Cart_Secure_Init(u32* buf, u32* out);
void Cart_Dummy(void);
