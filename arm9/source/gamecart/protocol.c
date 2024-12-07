// Copyright 2014 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <arm.h>

#include "protocol.h"
#include "timer.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "protocol_ctr.h"
#include "protocol_ntr.h"
#include "command_ctr.h"
#include "command_ntr.h"

// could have been done better, but meh...
#define REG_AESCNT      (*(vu32*)0x10009000)
#define REG_AESBLKCNT   (*(vu32*)0x10009004)
#define REG_AESWRFIFO   (*(vu32*)0x10009008)
#define REG_AESRDFIFO   (*(vu32*)0x1000900C)
#define REG_AESKEYSEL   (*(vu8*)0x10009010)
#define REG_AESKEYCNT   (*(vu8*)0x10009011)
#define REG_AESCTR      ((vu32*)0x10009020) // 16
#define REG_AESMAC      ((vu32*)0x10009030) // 16
#define REG_AESKEYFIFO  (*(vu32*)0x10009100)
#define REG_AESKEYXFIFO (*(vu32*)0x10009104)
#define REG_AESKEYYFIFO (*(vu32*)0x10009108)

u32 CartID = 0xFFFFFFFFu;
u32 CartType = 0;

static u32 CartID2 = 0xFFFFFFFFu;
static u32 rand1 = 0;
static u32 rand2 = 0;

// updated function by profi200
static void ResetCardSlot(void)
{
    REG_CARDCYCLES0 = 0x1988;
    REG_CARDCYCLES1 = 0x264C;
    // boot9 waits here. Unnecessary?

    REG_CARDSTATUS = 3u<<2;     // Request power off
    while(REG_CARDSTATUS != 0); // Aotomatically changes to 0 (off)
    wait_msec(1*2);

    REG_CARDSTATUS = 1u<<2;     // Prepare power on
    wait_msec(10*2);

    REG_CARDSTATUS = 2u<<2;     // Power on
    wait_msec(27*2);

    // Switch to NTRCARD controller.
    REG_CARDCTL = 0;          // Select NTRCARD controller, eject IRQ off?
    REG_NTRCARDMCNT = NTRCARD_CR1_ENABLE | NTRCARD_CR1_IRQ;
    REG_NTRCARDROMCNT = 0x20000000;
    wait_msec(120*2);
}

static void SwitchToNTRCARD(void)
{
    REG_NTRCARDROMCNT = 0x20000000;
    REG_CARDCTL &= ~3;
    REG_CARDCTL &= ~0x100;
    REG_NTRCARDMCNT = NTRCARD_CR1_ENABLE;
}

static void SwitchToCTRCARD(void)
{
    REG_CTRCARDCNT = 0x10000000;
    REG_CARDCTL = (REG_CARDCTL & ~3) | 2;
}

int Cart_IsInserted(void)
{
    return (0x9000E2C2 == CTR_CmdGetSecureId(rand1, rand2) );
}

u32 Cart_GetID(void)
{
    return CartID;
}

u32 Cart_GetID2(void)
{
    return CartID2;
}

void Cart_Init(void)
{
    ResetCardSlot(); //Seems to reset the cart slot?

    // Reset
    NTR_CmdReset();
    CartID = NTR_CmdGetCartId();

    // 3ds
    if (CartID & 0x10000000) {
        u32 getid2_cmd[2] = { 0xA0000000, 0x00000000 };
        NTR_SendCommand(getid2_cmd, 0x4, 0, &CartID2);

        NTR_CmdEnter16ByteMode();
        SwitchToCTRCARD();

        REG_CTRCARDBLKCNT = 0;
    }
}

static void AES_SetKeyControl(u32 a) {
    REG_AESKEYCNT = (REG_AESKEYCNT & 0xC0) | a | 0x80;
}

//returns 1 if MAC valid otherwise 0
static u8 card_aes(u32 *out, u32 *buff, size_t size) { // note size param ignored
    (void) size;
    REG_AESCNT = 0x10C00;    //flush r/w fifo macsize = 001

    (*(vu8*)0x10000008) |= 0x0C; //???

    REG_AESCNT |= 0x2800000;

    //const u8 is_dev_unit = *(vu8*)0x10010010;
    //if(is_dev_unit) //Dev unit
    const u8 is_dev_cart = (CartID2&3)==3;
    if(is_dev_cart) //Dev unit
    {
        AES_SetKeyControl(0x11);
        REG_AESKEYFIFO = 0;
        REG_AESKEYFIFO = 0;
        REG_AESKEYFIFO = 0;
        REG_AESKEYFIFO = 0;
        REG_AESKEYSEL = 0x11;
    }
    else
    {
        AES_SetKeyControl(0x3B);
        REG_AESKEYYFIFO = buff[0];
        REG_AESKEYYFIFO = buff[1];
        REG_AESKEYYFIFO = buff[2];
        REG_AESKEYYFIFO = buff[3];
        REG_AESKEYSEL = 0x3B;
    }

    REG_AESCNT = 0x4000000;
    REG_AESCNT &= 0xFFF7FFFF;
    REG_AESCNT |= 0x2970000;
    REG_AESMAC[0] = buff[11];
    REG_AESMAC[1] = buff[10];
    REG_AESMAC[2] = buff[9];
    REG_AESMAC[3] = buff[8];
    REG_AESCNT |= 0x2800000;
    REG_AESCTR[0] = buff[14];
    REG_AESCTR[1] = buff[13];
    REG_AESCTR[2] = buff[12];
    REG_AESBLKCNT = 0x10000;

    u32 v11 = ((REG_AESCNT | 0x80000000) & 0xC7FFFFFF); //Start and clear mode (ccm decrypt)
    u32 v12 = v11 & 0xBFFFFFFF; //Disable Interrupt
    REG_AESCNT = ((((v12 | 0x3000) & 0xFD7F3FFF) | (5 << 23)) & 0xFEBFFFFF) | (5 << 22);

    //REG_AESCNT = 0x83D73C00;
    REG_AESWRFIFO = buff[4];
    REG_AESWRFIFO = buff[5];
    REG_AESWRFIFO = buff[6];
    REG_AESWRFIFO = buff[7];
    while (((REG_AESCNT >> 5) & 0x1F) <= 3);
    out[0] = REG_AESRDFIFO;
    out[1] = REG_AESRDFIFO;
    out[2] = REG_AESRDFIFO;
    out[3] = REG_AESRDFIFO;
    return ((REG_AESCNT >> 21) & 1);
}

void Cart_Secure_Init(u32 *buf, u32 *out)
{
    card_aes(out, buf, 0x200);
//    u8 mac_valid = card_aes(out, buf, 0x200);

//    if (!mac_valid)
//        ClearScreen(bottomScreen, RGB(255, 0, 0));

    ARM_WaitCycles(0xF0000 * 8);

    CTR_SetSecKey(CartID2);
    CTR_SetSecSeed(out, true);

    rand1 = 0x42434445;//*((vu32*)0x10011000);
    rand2 = 0x46474849;//*((vu32*)0x10011010);

    CTR_CmdSeed(rand1, rand2);

    out[3] = BSWAP32(rand2);
    out[2] = BSWAP32(rand1);
    CTR_SetSecSeed(out, false);

    u32 test = 0;
    const u32 A2_cmd[4] = { 0xA2000000, 0x00000000, rand1, rand2 };
    CTR_SendCommand(A2_cmd, 4, 1, 0x701002C, &test);

    u32 test2 = 0;
    const u32 A3_cmd[4] = { 0xA3000000, 0x00000000, rand1, rand2 };
    CTR_SendCommand(A3_cmd, 4, 1, 0x701002C, &test2);

    if(test==CartID && test2==CartID2)
    {
        const u32 C5_cmd[4] = { 0xC5000000, 0x00000000, rand1, rand2 };
        CTR_SendCommand(C5_cmd, 0, 1, 0x100002C, NULL);
    }

    for (int i = 0; i < 5; ++i) {
        CTR_SendCommand(A2_cmd, 4, 1, 0x701002C, &test);
        ARM_WaitCycles(0xF0000 * 8);
    }
}

void Cart_Dummy(void) {
    // Sends a dummy command to skip encrypted responses some problematic carts send.
    u32 test;
    const u32 A2_cmd[4] = { 0xA2000000, 0x00000000, rand1, rand2 };
    CTR_SendCommand(A2_cmd, 4, 1, 0x701002C, &test);
}
