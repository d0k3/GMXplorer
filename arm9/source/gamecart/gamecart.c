#include "gamecart.h"
#include "protocol.h"
#include "command_ctr.h"
#include "command_ntr.h"
#include "card_spi.h"
#include "nds.h"
#include "ncch.h"
#include "ncsd.h"
#include "rtc.h"

#define CART_INSERTED (!(REG_CARDSTATUS & 0x1))

typedef struct {
    NcsdHeader ncsd;
    u32 card2_offset;
    u8  cinfo0[0x312 - (0x200 + sizeof(u32))];
    u32 rom_version;
    u8  cinfo1[0x1000 - (0x312 + sizeof(u32))];
    NcchHeader ncch;
    u8  padding[0x3000 - 0x200];
    u8  private[PRIV_HDR_SIZE];
    u8  unused[0x4000 + 0x8000 - PRIV_HDR_SIZE]; // 0xFF
    u32 cart_type;
    u32 cart_id;
    u32 cart_id2;
    u64 cart_size;
    u64 data_size;
    u32 save_size;
    CardSPIType save_type;
    u32 unused_offset;
} PACKED_ALIGN(16) CartDataCtr;

typedef struct {
    TwlHeader ntr_header;
    u8 ntr_padding[0x3000]; // 0x00
    u8 secure_area[0x4000];
    u8 secure_area_enc[0x4000];
    u8 modcrypt_area[0x4000];
    u32 cart_type;
    u32 cart_id;
    u32 cart_id2; // meaningless on TWL
    u64 cart_size;
    u64 data_size;
    u32 save_size;
    CardSPIType save_type;
    u32 arm9i_rom_offset;
} PACKED_ALIGN(16) CartDataNtrTwl;

static DsTime init_time;
static bool encrypted_sa = false;

u32 GetCartName(char* name, CartData* cdata) {
    if (cdata->cart_type & CART_CTR) {
        CartDataCtr* cdata_i = (CartDataCtr*)cdata;
        NcsdHeader* ncsd = &(cdata_i->ncsd);
        snprintf(name, 24, "%016llX_v%02lu", ncsd->mediaId, cdata_i->rom_version);
    }  else if (cdata->cart_type & CART_NTR) {
        CartDataNtrTwl* cdata_i = (CartDataNtrTwl*)cdata;
        TwlHeader* nds = &(cdata_i->ntr_header);
        snprintf(name, 24, "%.12s_%.6s_%02u", nds->game_title, nds->game_code, nds->rom_version);
    } else return 1;
    for (char* c = name; *c != '\0'; c++)
        if ((*c == ':') || (*c == '*') || (*c == '?') || (*c == '/') || (*c == '\\') || (*c == ' ')) *c = '_';
    return 0;
}

u32 GetCartInfoString(char* info, size_t info_size, CartData* cdata) {
    size_t info_index = 0;
    u8 padding;

    // read the last byte of the cart storage, but ignore the result
    ReadCartBytes(&padding, cdata->cart_size - 1, 1, cdata, false);

    if (cdata->cart_type & CART_CTR) {
        CartDataCtr* cdata_i = (CartDataCtr*)cdata;
        NcsdHeader* ncsd = &(cdata_i->ncsd);
        NcchHeader* ncch = &(cdata_i->ncch);
        info_index += snprintf(info + info_index, info_size - info_index,
            "Title ID     : %016llX\n"
            "Product Code : %.10s\n"
            "Revision     : %lu\n"
            "Cart ID      : %08lX\n"
            "Platform     : %s\n",
            ncsd->mediaId, ncch->productcode, cdata_i->rom_version, cdata_i->cart_id,
            (ncch->flags[4] == 0x2) ? "N3DS" : "O3DS");
    }  else if (cdata->cart_type & CART_NTR) {
        CartDataNtrTwl* cdata_i = (CartDataNtrTwl*)cdata;
        TwlHeader* nds = &(cdata_i->ntr_header);
        info_index += snprintf(info + info_index, info_size - info_index,
            "Title String : %.12s\n"
            "Product Code : %.6s\n"
            "Revision     : %u\n"
            "Cart ID      : %08lX\n"
            "Platform     : %s\n",
            nds->game_title, nds->game_code, nds->rom_version, cdata_i->cart_id,
            (nds->unit_code == 0x2) ? "DSi Enhanced" : (nds->unit_code == 0x3) ? "DSi Exclusive" : "DS");
    } else return 1;

    info_index += snprintf(info + info_index, info_size - info_index,
        "Save Type    : %s\n",
        (cdata->save_type == CARD_SAVE_NONE) ? "NONE" :
        (cdata->save_type == CARD_SAVE_SPI) ? "SPI" :
        (cdata->save_type == CARD_SAVE_CARD2) ? "CARD2" :
        (cdata->save_type == CARD_SAVE_RETAIL_NAND) ? "RETAIL_NAND" : "UNK");

    if (cdata->save_type == CARD_SAVE_SPI) {
        u32 jedecid = 0;
        if (CardSPIReadJEDECIDAndStatusReg(cdata->spi_save_type.infrared, &jedecid, NULL) == 0) {
            info_index += snprintf(info + info_index, info_size - info_index,
                "Save chip ID : %06lX\n",
                jedecid);
        }
    } else info_index += snprintf(info + info_index, info_size - info_index,
        "Save chip ID : <none>\n");

    info_index += snprintf(info + info_index, info_size - info_index,
        "Padding Byte : %02X\n"
        "Timestamp    : 20%02X-%02X-%02X %02X:%02X:%02X\n"
        "GM9 Version  : %s\n",
        padding,
        init_time.bcd_Y, init_time.bcd_M, init_time.bcd_D,
        init_time.bcd_h, init_time.bcd_m, init_time.bcd_s,
        VERSION);
    return 0;
}

u32 SetSecureAreaEncryption(bool encrypted) {
    encrypted_sa = encrypted;
    return 0;
}

static u32 GetCtrCartSaveSize(CartData* cdata) {
    NcsdHeader* ncsd = (NcsdHeader*) (void*) cdata->header;
    u32 ncch_sector = ncsd->partitions[0].offset;

    // Load header and ExHeader for first partition
    u8 buffer[0x400];
    CTR_CmdReadData(ncch_sector, 0x200, 2, buffer);
    NcchHeader* ncch = (NcchHeader*) (void*) buffer;
    if (ValidateNcchHeader(ncch) != 0) {
        return 0;
    }

    // Ensure first partition has ExHeader
    if (ncch->size_exthdr < 0x200) {
        return 0;
    }

    // Decrypt ExHeader
    if ((NCCH_ENCRYPTED(ncch)) && (SetupNcchCrypto(ncch, NCCH_NOCRYPTO) == 0)) {
        DecryptNcch(buffer + NCCH_EXTHDR_OFFSET, NCCH_EXTHDR_OFFSET, sizeof(buffer) - NCCH_EXTHDR_OFFSET, ncch, NULL);
    }
    u64 savesize = getle64(buffer + NCCH_EXTHDR_OFFSET + 0x1C0);

    // check our work
    if (savesize <= UINT32_MAX) {
        return (u32) savesize;
    } else {
        return 0;
    }
}

u32 InitCartRead(CartData* cdata) {
    get_dstime(&init_time);
    encrypted_sa = false;
    memset(cdata, 0x00, sizeof(CartData));
    cdata->cart_type = CART_NONE;
    if (!CART_INSERTED) return 1;
    Cart_Init();
    cdata->cart_id = Cart_GetID();
    cdata->cart_type = (cdata->cart_id & 0x10000000) ? CART_CTR : CART_NTR;

    // Use the cart ID to determine the ROM size.
    // (ROM header might be incorrect on dev carts.)
    switch ((cdata->cart_id >> 16) & 0xFF) {
        case 0x07:  cdata->cart_size = 8ULL*1024*1024; break;
        case 0x0F:  cdata->cart_size = 16ULL*1024*1024; break;
        case 0x1F:  cdata->cart_size = 32ULL*1024*1024; break;
        case 0x3F:  cdata->cart_size = 64ULL*1024*1024; break;
        case 0x7F:  cdata->cart_size = 128ULL*1024*1024; break;
        case 0xFF:  cdata->cart_size = 256ULL*1024*1024; break;
        case 0xFE:  cdata->cart_size = 512ULL*1024*1024; break;
        case 0xFA:  cdata->cart_size = 1024ULL*1024*1024; break;
        case 0xF8:  cdata->cart_size = 2048ULL*1024*1024; break;
        case 0xF0:  cdata->cart_size = 4096ULL*1024*1024; break;
        default:    cdata->cart_size = 0; break;
    }

    if (cdata->cart_type & CART_CTR) { // CTR cartridges
        memset(cdata, 0xFF, 0x4000 + PRIV_HDR_SIZE); // switch the padding to 0xFF

        // init, NCCH header
        static u32 sec_keys[4];
        u8* ncch_header = cdata->header + 0x1000;
        cdata->cart_id2 = Cart_GetID2();
        CTR_CmdReadHeader(ncch_header);
        Cart_Secure_Init((u32*) (void*) ncch_header, sec_keys);

        // NCSD header and CINFO
        // Cart_Dummy();
        // Cart_Dummy();
        CTR_CmdReadData(0, 0x200, 8, cdata->header);

        // safety checks, cart size
        NcsdHeader* ncsd = (NcsdHeader*) (void*) cdata->header;
        NcchHeader* ncch = (NcchHeader*) (void*) ncch_header;
        if ((ValidateNcsdHeader(ncsd) != 0) || (ValidateNcchHeader(ncch) != 0))
            return 1;
        if (cdata->cart_size == 0)
            cdata->cart_size = (u64) ncsd->size * NCSD_MEDIA_UNIT;
        cdata->data_size = GetNcsdTrimmedSize(ncsd);
        if (cdata->cart_size > 0x100000000) return 1; // carts > 4GB don't exist
        // else if (cdata->cart_size == 0x100000000) cdata->cart_size -= 0x200; // silent 4GB fix
        if (cdata->data_size > cdata->cart_size) return 1;

        // private header
        u8* priv_header = cdata->header + 0x4000;
        CTR_CmdReadUniqueID(priv_header);
        memcpy(priv_header + 0x40, &(cdata->cart_id), 4);
        memcpy(priv_header + 0x44, &(cdata->cart_id2), 4);
        memset(priv_header + 0x48, 0xFF, 8);

        // save data
        u32 card2_offset = getle32(cdata->header + 0x200);
        if (card2_offset != 0xFFFFFFFF) {
            cdata->save_type = CARD_SAVE_CARD2;
            cdata->save_size = GetCtrCartSaveSize(cdata);
            // Sanity checks
            if ((cdata->save_size == 0) ||
                (card2_offset * NCSD_MEDIA_UNIT >= cdata->cart_size) ||
                (card2_offset * NCSD_MEDIA_UNIT + cdata->save_size > cdata->cart_size)) {
                cdata->save_type = CARD_SAVE_NONE;
            }
        } else {
            cdata->spi_save_type = (CardSPIType) { FLASH_CTR_GENERIC, false };
            cdata->save_size = CardSPIGetCapacity(cdata->spi_save_type);
            if (cdata->save_size == 0) {
                cdata->spi_save_type = (CardSPIType) { NO_CHIP, false };
            }
            if (cdata->spi_save_type.chip == NO_CHIP) {
                cdata->save_type = CARD_SAVE_NONE;
            } else {
                cdata->save_type = CARD_SAVE_SPI;
                cdata->save_size = CardSPIGetCapacity(cdata->spi_save_type);
            }
        }
    } else { // NTR/TWL cartridges
        // NTR header
        TwlHeader* nds_header = (void*)cdata->header;
        u8 secure_area_enc[0x4000];
        NTR_CmdReadHeader(cdata->header);
        if (!(*(cdata->header))) return 1; // error reading the header
        if (!NTR_Secure_Init(cdata->header, secure_area_enc, Cart_GetID(), 0)) return 1;


        // cartridge size, trimmed size, twl presets
        if (nds_header->device_capacity >= 15) return 1; // too big, not valid
        if (cdata->cart_size == 0)
            cdata->cart_size = (128 * 1024) << nds_header->device_capacity;
        cdata->data_size = nds_header->ntr_rom_size;
        cdata->arm9i_rom_offset = 0;

        // TWL header
        if (nds_header->unit_code != 0x00) { // DSi or NDS+DSi
            cdata->cart_type |= CART_TWL;
            cdata->data_size = nds_header->ntr_twl_rom_size;
            cdata->arm9i_rom_offset = nds_header->arm9i_rom_offset;
            if ((cdata->arm9i_rom_offset < nds_header->ntr_rom_size) ||
                (cdata->arm9i_rom_offset + MODC_AREA_SIZE > cdata->data_size))
                return 1; // safety first

            // Some NTR dev carts have TWL ROMs flashed to them.
            // We'll only want to use TWL secure init if this is a TWL cartridge.
            if (cdata->cart_id & 0x40000000U) { // TWL cartridge
                Cart_Init();
                NTR_CmdReadHeader(cdata->storage);
                if (!NTR_Secure_Init(cdata->storage, NULL, Cart_GetID(), 1)) return 1;
            }
        } else {
            // Check if immediately after the reported cart size
            // is the magic number string 'ac' (auth code).
            // If found, add 0x88 bytes for the download play RSA key.
            u16 rsaMagic;
            ReadCartBytes(&rsaMagic, cdata->data_size, 2, cdata, false);
            if(rsaMagic == 0x6361) {
                cdata->data_size += 0x88;
            }
        }

        // store encrypted secure area
        memcpy(cdata->storage, secure_area_enc, 0x4000);

        // last safety check
        if (cdata->data_size > cdata->cart_size) return 1;

        // save data
        bool infrared = *(nds_header->game_code) == 'I';
        cdata->spi_save_type = CardSPIGetCardSPIType(infrared);
        if (cdata->spi_save_type.chip == NO_CHIP) {
            cdata->save_type = CARD_SAVE_NONE;
        } else {
            cdata->save_type = CARD_SAVE_SPI;
            cdata->save_size = CardSPIGetCapacity(cdata->spi_save_type);
        }
    }
    return 0;
}

u32 ReadCartSectors(void* buffer, u32 sector, u32 count, CartData* cdata, bool card2_blanking) {
    u8* buffer8 = (u8*) buffer;
    if (!CART_INSERTED) return 1;
    // header
    const u32 header_sectors = 0x4000/0x200;
    if (sector < header_sectors) {
        u32 header_count = (sector + count > header_sectors) ? header_sectors - sector : count;
        memcpy(buffer8, cdata->header + (sector * 0x200), header_count * 0x200);
        buffer8 += header_count * 0x200;
        sector += header_count;
        count -= header_count;
    }
    if (!count) return 0;
    // actual cart reads
    if (cdata->cart_type & CART_CTR) {
        // don't read more than 1MB at once
        const u32 max_read = 0x800;
        u8* buff = buffer8;
        for (u32 i = 0; i < count; i += max_read) {
            // Cart_Dummy();
            // Cart_Dummy();
            CTR_CmdReadData(sector + i, 0x200, min(max_read, count - i), buff);
            buff += max_read * 0x200;
        }

        // overwrite the card2 savegame with 0xFF
        u32 card2_offset = getle32(cdata->header + 0x200);
        u32 save_sectors = cdata->save_size / 0x200;
        if (card2_blanking &&
            (cdata->save_type == CARD_SAVE_CARD2) &&
            ((card2_offset * 0x200) >= cdata->data_size) &&
            (sector + count > card2_offset) && // requested area ends after the save starts
            (sector < card2_offset + save_sectors)) { // requested area starts before the save ends
            u32 blank_start_sector, blank_end_sector;
            if (sector > card2_offset) {
                blank_start_sector = sector;
            } else {
                blank_start_sector = card2_offset;
            }
            if (sector + count < card2_offset + save_sectors) {
                blank_end_sector = sector + count;
            } else {
                blank_end_sector = card2_offset + save_sectors;
            }

            memset(buffer8 + (blank_start_sector - sector) * 0x200, 0xFF,
                (blank_end_sector - blank_start_sector) * 0x200);
        }
    } else if (cdata->cart_type & CART_NTR) {
        u8* buff = buffer8;

        // secure area handling
        const u32 sa_sector_end = 0x8000/0x200;
        if (sector < sa_sector_end) {
            CartDataNtrTwl* cdata_twl = (CartDataNtrTwl*) cdata;
            u8* sa = encrypted_sa ? cdata_twl->secure_area_enc : cdata_twl->secure_area;
            u32 count_sa = ((sector + count) > sa_sector_end) ?  sa_sector_end - sector : count;
            memcpy(buff, sa + ((sector - header_sectors) * 0x200), count_sa * 0x200);
            buff += count_sa * 0x200;
            sector += count_sa;
            count -= count_sa;
        }

        // regular cart data
        u32 off = sector * 0x200;
        for (u32 i = 0; i < count; i++, off += 0x200, buff += 0x200)
            NTR_CmdReadData(off, buff);

        // modcrypt area handling
        if ((cdata->cart_type & CART_TWL) &&
            ((sector+count) * 0x200 > cdata->arm9i_rom_offset) &&
            (sector * 0x200 < cdata->arm9i_rom_offset + MODC_AREA_SIZE)) {
            u32 arm9i_rom_offset = cdata->arm9i_rom_offset;
            u8* buffer_arm9i = buffer8;
            u32 offset_i = 0;
            u32 size_i = MODC_AREA_SIZE;
            if (arm9i_rom_offset < (sector * 0x200))
                offset_i = (sector * 0x200) - arm9i_rom_offset;
            else buffer_arm9i = buffer8 + (arm9i_rom_offset - (sector * 0x200));
            size_i = MODC_AREA_SIZE - offset_i;
            if (size_i > (count * 0x200) - (buffer_arm9i - buffer8))
                size_i = (count * 0x200) - (buffer_arm9i - buffer8);
            if (size_i) memcpy(buffer_arm9i, cdata->storage + 0x4000 + offset_i, size_i);
        }
    } else return 1;
    return 0;
}

u32 ReadCartBytes(void* buffer, u64 offset, u64 count, CartData* cdata, bool card2_blanking) {
    if (!(offset % 0x200) && !(count % 0x200)) { // aligned data -> simple case
        // simple wrapper function for ReadCartSectors(...)
        return ReadCartSectors(buffer, offset / 0x200, count / 0x200, cdata, card2_blanking);
    } else { // misaligned data -> -___-
        u8* buffer8 = (u8*) buffer;
        u8 l_buffer[0x200];
        if (offset % 0x200) { // handle misaligned offset
            u32 offset_fix = 0x200 - (offset % 0x200);
            if (ReadCartSectors(l_buffer, offset / 0x200, 1, cdata, card2_blanking) != 0) return 1;
            memcpy(buffer8, l_buffer + 0x200 - offset_fix, min(offset_fix, count));
            if (count <= offset_fix) return 0;
            offset += offset_fix;
            buffer8 += offset_fix;
            count -= offset_fix;
        } // offset is now aligned and part of the data is read
        if (count >= 0x200) { // otherwise this is misaligned and will be handled below
            if (ReadCartSectors(buffer8, offset / 0x200, count / 0x200, cdata, card2_blanking) != 0) return 1;
        }
        if (count % 0x200) { // handle misaligned count
            u32 count_fix = count % 0x200;
            if (ReadCartSectors(l_buffer, (offset + count) / 0x200, 1, cdata, card2_blanking) != 0) return 1;
            memcpy(buffer8 + count - count_fix, l_buffer, count_fix);
        }
        return 0;
    }
}

u32 ReadCartPrivateHeader(void* buffer, u64 offset, u64 count, CartData* cdata) {
    if (!(cdata->cart_type & CART_CTR)) return 1;
    if (offset < PRIV_HDR_SIZE) {
        u8* priv_hdr = cdata->header + 0x4000;
        if (offset + count > PRIV_HDR_SIZE) count = PRIV_HDR_SIZE - offset;
        memcpy(buffer, priv_hdr + offset, count);
    }
    return 0;
}

u32 ReadCartInfo(u8* buffer, u64 offset, u64 count, CartData* cdata) {
    char info[256];
    u32 len;

    GetCartInfoString(info, sizeof(info), cdata);
    len = strnlen(info, 255);

    if (offset >= len) return 0;
    if (offset + count > len) count = len - offset;
    memcpy(buffer, info + offset, count);

    return 0;
}

u32 ReadCartSave(u8* buffer, u64 offset, u64 count, CartData* cdata) {
    if (offset >= cdata->save_size) return 1;
    if (offset + count > cdata->save_size) count = cdata->save_size - offset;
    switch (cdata->save_type) {
        case CARD_SAVE_SPI:
            return (CardSPIReadSaveData(cdata->spi_save_type, offset, buffer, count) == 0) ? 0 : 1;
            break;

        case CARD_SAVE_CARD2:
        {
            u32 card2_offset = getle32(cdata->header + 0x200);
            return ReadCartBytes(buffer, card2_offset * NCSD_MEDIA_UNIT + offset, count, cdata, false);
            break;
        }

        default:
            return 1;
            break;
    }
}

u32 WriteCartSave(const u8* buffer, u64 offset, u64 count, CartData* cdata) {
    if (offset >= cdata->save_size) return 1;
    if (offset + count > cdata->save_size) count = cdata->save_size - offset;
    return (CardSPIWriteSaveData(cdata->spi_save_type, offset, buffer, count) == 0) ? 0 : 1;
}
