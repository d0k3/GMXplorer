#include "filetype.h"
#include "fsutil.h"
#include "image.h"
#include "fatmbr.h"
#include "nand.h"
#include "game.h"
#include "disadiff.h"
#include "keydb.h"
#include "ctrtransfer.h"
#include "scripting.h"
#include "gm9lua.h"
#include "png.h"
#include "ui.h" // only for font file detection

u64 IdentifyFileType(const char* path) {
    static const u8 romfs_magic[] = { ROMFS_MAGIC };
    static const u8 diff_magic[] = { DIFF_MAGIC };
    static const u8 disa_magic[] = { DISA_MAGIC };
    static const u8 tickdb_magic[] = { TICKDB_MAGIC };
    static const u8 smdh_magic[] = { SMDH_MAGIC };
    static const u8 threedsx_magic[] = { THREEDSX_EXT_MAGIC };
    static const u8 png_magic[] = { PNG_MAGIC };

    if (!path) return 0; // safety
    u8 ALIGN(32) header[0x2C0]; // minimum required size
    void* data = (void*) header;
    size_t fsize = FileGetSize(path);
    char* fname = strrchr(path, '/');
    char* ext = (fname) ? strrchr(++fname, '.') : NULL;
    u32 id = 0;

    // block crappy "._" files from getting recognized as filetype
    if (!fname) return 0;
    if (strncmp(fname, "._", 2) == 0) return 0;

    if (ext) {
        ext++;
    } else {
        ext = "";
    }
    if (FileGetData(path, header, 0x2C0, 0) < min(0x2C0, fsize)) return 0;
    if (!fsize) return 0;

    if (fsize >= 0x200) {
        if (ValidateNandNcsdHeader((NandNcsdHeader*) data) == 0) {
            return (fsize >= GetNandNcsdMinSizeSectors((NandNcsdHeader*) data) * 0x200) ?
                IMG_NAND : (fsize == sizeof(NandNcsdHeader)) ? HDR_NAND : 0; // NAND image or just header
        } else if ((strncasecmp(path, "S:/nand.bin", 16) == 0) || (strncasecmp(path, "E:/nand.bin", 16) == 0)) {
            return NOIMG_NAND; // on NAND, but no proper NAND image
        } else if (ValidateFatHeader(header) == 0) {
            return IMG_FAT; // FAT image file
        } else if (ValidateMbrHeader((MbrHeader*) data) == 0) {
            MbrHeader* mbr = (MbrHeader*) data;
            MbrPartitionInfo* partition0 = mbr->partitions;
            bool ctr = (CheckTransferableMbr(mbr) == 0); // is this a CTRNAND MBR?
            if ((partition0->sector + partition0->count) <= (fsize / 0x200)) // size check
                return IMG_FAT | (ctr ? FLAG_CTR : 0); // possibly an MBR -> also treat as FAT image
        } else if (ValidateCiaHeader((CiaHeader*) data) == 0) {
            // this only works because these functions ignore CIA content index
            CiaInfo info;
            GetCiaInfo(&info, data);
            if (fsize >= info.size_cia)
                return GAME_CIA; // CIA file
        } else if (ValidateNcsdHeader((NcsdHeader*) data) == 0) {
            NcsdHeader* ncsd = (NcsdHeader*) data;
            if (fsize >= GetNcsdTrimmedSize(ncsd))
                return GAME_NCSD; // NCSD (".3DS") file
        } else if (ValidateNcchHeader((NcchHeader*) data) == 0) {
            NcchHeader* ncch = (NcchHeader*) data;
            u64 type = GAME_NCCH;
            if (NCCH_IS_CXI(ncch)) {
                type |= FLAG_CXI;
                /* the below is unused for now
                type |= NCCH_IS_FIRM(ncch) ? FLAG_FIRM : 0;
                NcchExtHeader exhdr;
                if ((FileGetData(path, &exhdr, 0x400, 0x200) == 0x400) && // read only what we need
                    (DecryptNcch(&exhdr, 0x200, 0x400, ncch, NULL) == 0) &&
                    NCCH_IS_GBAVC(&exhdr))
                    type |= FLAG_GBAVC;
                */
            }
            if (fsize >= (ncch->size * NCCH_MEDIA_UNIT))
                return type; // NCCH (".APP") file
        } else if (ValidateExeFsHeader((ExeFsHeader*) data, fsize) == 0) {
            return GAME_EXEFS; // ExeFS file (false positives possible)
        } else if (memcmp(header, romfs_magic, sizeof(romfs_magic)) == 0) {
            return GAME_ROMFS; // RomFS file (check could be better)
        } else if (ValidateTmd((TitleMetaData*) data) == 0) {
            if (fsize == TMD_SIZE_N(getbe16(header + 0x1DE)) + TMD_CDNCERT_SIZE)
                return GAME_CDNTMD; // TMD file from NUS/CDN
            else if (fsize >= TMD_SIZE_N(getbe16(header + 0x1DE)))
                return GAME_TMD; // TMD file
        } else if (ValidateTwlTmd((TitleMetaData*) data) == 0) {
            if (fsize == TMD_SIZE_TWL + TMD_CDNCERT_SIZE)
                return GAME_TWLTMD; // TMD file from NUS/CDN (TWL)
        } else if (ValidateTicket((Ticket*) data) == 0) {
            return GAME_TICKET; // Ticket file
        } else if (ValidateFirmHeader((FirmHeader*) data, fsize) == 0) {
            return SYS_FIRM; // FIRM file
        } else if ((ValidateAgbSaveHeader((AgbSaveHeader*) data) == 0) && (fsize >= AGBSAVE_MAX_SIZE)) {
            return SYS_AGBSAVE; // AGBSAVE file
        } else if (memcmp(header + 0x100, diff_magic, sizeof(diff_magic)) == 0) { // DIFF file
            if (memcmp(header + 0x100, tickdb_magic, sizeof(tickdb_magic)) == 0) // ticket.db file
                return SYS_DIFF | SYS_TICKDB; // ticket.db
            return SYS_DIFF;
        } else if (memcmp(header + 0x100, disa_magic, sizeof(disa_magic)) == 0) { // DISA file
            return SYS_DISA;
        } else if (memcmp(header, smdh_magic, sizeof(smdh_magic)) == 0) {
            return GAME_SMDH; // SMDH file
        } else if (ValidateTwlHeader((TwlHeader*) data) == 0) {
            TwlHeader* twl = (TwlHeader*) data;
            if (twl->ntr_rom_size <= fsize) { // NDS rom file
                if ((twl->unit_code == 0x03) && !twl->twl_rom_region_start)
                    return GAME_NDS | FLAG_DSIW; // NDS DSiWare rom file
                return GAME_NDS; // NDS rom file
            }
        }
    }

    if (fsize == sizeof(TitleInfoEntry) && (strncasecmp(path, "T:/", 3) == 0)) {
        const char* mntpath = GetMountPath();
        if (mntpath && *mntpath) {
            if ((strncasecmp(mntpath, "1:/dbs/title.db", 16) == 0) ||
                (strncasecmp(mntpath, "4:/dbs/title.db", 16) == 0) ||
                (strncasecmp(mntpath, "A:/dbs/title.db", 16) == 0) ||
                (strncasecmp(mntpath, "B:/dbs/title.db", 16) == 0))
                return GAME_TIE;
        }
    } else if (GetFontFromPbm(data, fsize, NULL, NULL)) {
        return FONT_PBM;
    } else if (GetFontFromRiff(data, fsize, NULL, NULL, NULL)) {
        return FONT_RIFF;
    } else if (GetLanguage(data, fsize, NULL, NULL, NULL)) {
        return TRANSLATION;
    } else if ((fsize > sizeof(AgbHeader)) &&
        (ValidateAgbHeader((AgbHeader*) data) == 0)) {
        return GAME_GBA;
    } else if ((fsize > sizeof(BossHeader)) &&
        (ValidateBossHeader((BossHeader*) data, fsize) == 0)) {
        return GAME_BOSS; // BOSS (SpotPass) file
    } else if ((fsize > sizeof(ThreedsxHeader)) &&
        (memcmp(data, threedsx_magic, sizeof(threedsx_magic)) == 0)) {
        return GAME_3DSX; // 3DSX (executable) file
    } else if ((fsize > sizeof(CmdHeader)) &&
        (CMD_SIZE((CmdHeader*) data) == fsize)) {
        return GAME_CMD; // CMD file
    } else if ((fsize > sizeof(NcchInfoHeader)) &&
        (GetNcchInfoVersion((NcchInfoHeader*) data)) &&
        (strncasecmp(fname, NCCHINFO_NAME, 32) == 0)) {
        return BIN_NCCHNFO; // ncchinfo.bin file
    } else if ((strncasecmp(ext, "png", 4) == 0) &&
        (fsize > sizeof(png_magic)) && (memcmp(data, png_magic, sizeof(png_magic)) == 0)) {
        return GFX_PNG;
    } else if (strncasecmp(fname, TIKDB_NAME_ENC, sizeof(TIKDB_NAME_ENC)+1) == 0) {
        return BIN_TIKDB | FLAG_ENC; // titlekey database / encrypted
    } else if (strncasecmp(fname, TIKDB_NAME_DEC, sizeof(TIKDB_NAME_DEC)+1) == 0) {
        return BIN_TIKDB; // titlekey database / decrypted
    } else if (strncasecmp(fname, KEYDB_NAME, sizeof(KEYDB_NAME)+1) == 0) {
        return BIN_KEYDB; // key database
    } else if ((sscanf(fname, "slot%02lXKey", &id) == 1) && (strncasecmp(ext, "bin", 4) == 0) && (fsize = 16) && (id < 0x40)) {
        return BIN_LEGKEY; // legacy key file
    } else if ((strncmp((char*) data, CIFINISH_MAGIC, strlen(CIFINISH_MAGIC)) == 0) &&
        (fsize == CIFINISH_SIZE((void*) data)) && (fsize > sizeof(CifinishHeader))) {
        return BIN_CIFNSH;
    } else if (ValidateText((char*) data, (fsize > 0x200) ? 0x200 : fsize)) {
        u64 type = 0;
        if ((fsize < SCRIPT_MAX_SIZE) && (strcasecmp(ext, SCRIPT_EXT) == 0))
            type |= TXT_SCRIPT; // should be a script (which is also generic text)
	    // this should check if it's compiled lua bytecode (done with luac), which is NOT text
	    else if ((fsize < LUASCRIPT_MAX_SIZE) && (strcasecmp(ext, LUASCRIPT_EXT) == 0))
            type |= TXT_LUA;
        if (fsize < STD_BUFFER_SIZE) type |= TXT_GENERIC;
        return type;
    } else if ((strncmp(path + 2, "/Nintendo DSiWare/", 18) == 0) &&
        (sscanf(fname, "%08lx.bin", &id) == 1) && (strncasecmp(ext, "bin", 4) == 0)) {
        TadHeader hdr;
        if ((FileGetData(path, &hdr, TAD_HEADER_LEN, TAD_HEADER_OFFSET) == TAD_HEADER_LEN) &&
            (strncmp(hdr.magic, TAD_HEADER_MAGIC, strlen(TAD_HEADER_MAGIC)) == 0))
            return GAME_TAD;
    } else if ((strnlen(fname, 16) == 8) && (sscanf(fname, "%08lx", &id) == 1)) {
        char path_cdn[256];
        char* name_cdn = path_cdn + (fname - path);
        strncpy(path_cdn, path, 256);
        path_cdn[255] = '\0';
        strncpy(name_cdn, "tmd", 4); // this will not catch tmd with version
        if (FileGetSize(path_cdn) > 0)
            return GAME_NUSCDN; // NUS/CDN, recognized by TMD
        strncpy(name_cdn, "cetk", 5);
        if (FileGetSize(path_cdn) > 0)
            return GAME_NUSCDN; // NUS/CDN, recognized by CETK
    }

    return 0;
}
