#include "filetype.h"
#include "fsutil.h"
#include "fatmbr.h"
#include "nand.h"
#include "game.h"
#include "chainload.h"
#include "movable.h"

u32 IdentifyFileType(const char* path) {
    const u8 romfs_magic[] = { ROMFS_MAGIC };
    const u8 tickdb_magic[] = { TICKDB_MAGIC };
    const u8 smdh_magic[] = { SMDH_MAGIC };
    const u8 movable_magic[] = { MOVABLE_MAGIC };
    u8 header[0x200] __attribute__((aligned(32))); // minimum required size
    void* data = (void*) header;
    size_t fsize = FileGetSize(path);
    char* fname = strrchr(path, '/');
    char* ext = (fname) ? strrchr(++fname, '.') : NULL;
    u32 id = 0;
    if (ext) ext++;
    if (FileGetData(path, header, 0x200, 0) < ((fsize > 0x200) ? 0x200 : fsize)) return 0;
    
    if (!fsize) return 0;
    if (fsize >= 0x200) {
        if ((getbe32(header + 0x100) == 0x4E435344) && (getbe64(header + 0x110) == (u64) 0x0104030301000000) &&
            (getbe64(header + 0x108) == (u64) 0) && (fsize >= 0x8FC8000)) {
            return IMG_NAND; // NAND image
        } else if (ValidateFatHeader(header) == 0) {
            return IMG_FAT; // FAT image file
        } else if (ValidateMbrHeader((MbrHeader*) data) == 0) {
            MbrHeader* mbr = (MbrHeader*) data;
            MbrPartitionInfo* partition0 = mbr->partitions;
            bool ctr = (CheckNandMbr(data) & (NAND_TYPE_O3DS|NAND_TYPE_N3DS)); // is this a CTRNAND MBR?
            if ((partition0->sector + partition0->count) <= (fsize / 0x200)) // size check
                return IMG_FAT | (ctr ? FLAG_CTR : 0); // possibly an MBR -> also treat as FAT image
        } else if (ValidateCiaHeader((CiaHeader*) data) == 0) {
            // this only works because these functions ignore CIA content index
            CiaInfo info;
            GetCiaInfo(&info, (CiaHeader*) header);
            if (fsize >= info.size_cia)
                return GAME_CIA; // CIA file
        } else if (ValidateNcsdHeader((NcsdHeader*) data) == 0) {
            NcsdHeader* ncsd = (NcsdHeader*) data;
            if (fsize >= GetNcsdTrimmedSize(ncsd))
                return GAME_NCSD; // NCSD (".3DS") file
        } else if (ValidateNcchHeader((NcchHeader*) data) == 0) {
            NcchHeader* ncch = (NcchHeader*) data;
            u32 type = GAME_NCCH | (NCCH_IS_CXI(ncch) ? FLAG_CXI : 0);
            if (fsize >= (ncch->size * NCCH_MEDIA_UNIT))
                return type; // NCCH (".APP") file
        } else if (ValidateExeFsHeader((ExeFsHeader*) data, fsize) == 0) {
            return GAME_EXEFS; // ExeFS file (false positives possible)
        } else if (memcmp(header, romfs_magic, sizeof(romfs_magic)) == 0) {
            return GAME_ROMFS; // RomFS file (check could be better)
        } else if (ValidateTmd((TitleMetaData*) data) == 0) {
            if (fsize == TMD_SIZE_N(getbe16(header + 0x1DE)) + TMD_CDNCERT_SIZE)
                return GAME_TMD | FLAG_NUSCDN; // TMD file from NUS/CDN
            else if (fsize >= TMD_SIZE_N(getbe16(header + 0x1DE)))
                return GAME_TMD; // TMD file
        } else if (ValidateTicket((Ticket*) data) == 0) {
            return GAME_TICKET; // Ticket file (not used for anything right now)
        } else if (ValidateFirmHeader((FirmHeader*) data, fsize) == 0) {
            return SYS_FIRM; // FIRM file
        } else if (memcmp(header + 0x100, tickdb_magic, sizeof(tickdb_magic)) == 0) {
            return SYS_TICKDB; // ticket.db
        } else if (memcmp(header, smdh_magic, sizeof(smdh_magic)) == 0) {
            return GAME_SMDH; // SMDH file
        } else if (ValidateTwlHeader((TwlHeader*) data) == 0) {
            return GAME_NDS; // NDS rom file
        }
    }
    if (memcmp(header, movable_magic, sizeof(movable_magic)) == 0) { // movable.sed
      return SYS_MOVABLE;
    } else if ((fsize > sizeof(BossHeader)) &&
        (ValidateBossHeader((BossHeader*) data, fsize) == 0)) {
        return GAME_BOSS; // BOSS (SpotPass) file
    } else if ((fsize > sizeof(NcchInfoHeader)) &&
        (GetNcchInfoVersion((NcchInfoHeader*) data)) &&
        fname && (strncasecmp(fname, NCCHINFO_NAME, 32) == 0)) {
        return BIN_NCCHNFO; // ncchinfo.bin file
    } else if ((strnlen(fname, 16) == 8) && (sscanf(fname, "%08lx", &id) == 1)) {
        char path_cdn[256];
        char* name_cdn = path_cdn + (fname - path);
        strncpy(path_cdn, path, 256);
        strncpy(name_cdn, "tmd", 4);
        if (FileGetSize(path_cdn) > 0)
            return GAME_NUSCDN; // NUS/CDN type 1
        strncpy(name_cdn, "cetk", 5);
        if (FileGetSize(path_cdn) > 0)
            return GAME_NUSCDN; // NUS/CDN type 1
    } else if (ext && ((strncasecmp(ext, "cdn", 4) == 0) || (strncasecmp(ext, "nus", 4) == 0))) {
        char path_cetk[256];
        char* ext_cetk = path_cetk + (ext - path);
        strncpy(ext_cetk, "cetk", 5);
        if (FileGetSize(path_cetk) > 0)
            return GAME_NUSCDN; // NUS/CDN type 2
    } else if ((strncasecmp(fname, "seeddb.bin", 11) == 0) ||
        (strncasecmp(fname, "encTitlekeys.bin", 17) == 0) ||
        (strncasecmp(fname, "decTitlekeys.bin", 17) == 0) ||
        (strncasecmp(fname, "aeskeydb.bin", 13) == 0) ||
        (strncasecmp(fname, "otp.bin", 8) == 0) ||
        (strncasecmp(fname, "secret_sector.bin", 18) == 0) ||
        (strncasecmp(fname, "sector0x96.bin", 15) == 0)) {
        return BIN_SUPPORT; // known support file (so launching is not offered)
    #if PAYLOAD_MAX_SIZE <= TEMP_BUFFER_SIZE
    } else if ((fsize <= PAYLOAD_MAX_SIZE) && ext && (strncasecmp(ext, "bin", 4) == 0)) {
        return BIN_LAUNCH; // assume it's an ARM9 payload
    #endif
    }
    
    return 0;
}
