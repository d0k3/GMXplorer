#pragma once

#include "common.h"
#include "sddata.h"
#include "ff.h"

#define AM_VRT 0x40 // Virtual (FILINFO FAT attribute)

#define fvx_tell(fp) ((fp)->fptr)
#define fvx_size(fp) ((fp)->obj.objsize)
#define fvx_eof(fp) (fvx_tell(fp) == fvx_size(fp))

#define FN_ANY      0x00
#define FN_HIGHEST  0x01
#define FN_LOWEST   0x02

// wrapper functions for ff.h + sddata.h
// incomplete(!) extension to FatFS to support a common interface for virtual and FAT
FRESULT fvx_open (FIL* fp, const TCHAR* path, BYTE mode);
FRESULT fvx_read (FIL* fp, void* buff, UINT btr, UINT* br);
FRESULT fvx_write (FIL* fp, const void* buff, UINT btw, UINT* bw);
FRESULT fvx_close (FIL* fp);
FRESULT fvx_lseek (FIL* fp, FSIZE_t ofs);
FRESULT fvx_sync (FIL* fp);
FRESULT fvx_stat (const TCHAR* path, FILINFO* fno);
FRESULT fvx_rename (const TCHAR* path_old, const TCHAR* path_new);
FRESULT fvx_unlink (const TCHAR* path);
FRESULT fvx_mkdir (const TCHAR* path);
FRESULT fvx_opendir (DIR* dp, const TCHAR* path);
FRESULT fvx_closedir (DIR* dp);
FRESULT fvx_readdir (DIR* dp, FILINFO* fno);

// additional quick read / write / create functions
FRESULT fvx_qread (const TCHAR* path, void* buff, FSIZE_t ofs, UINT btr, UINT* br);
FRESULT fvx_qwrite (const TCHAR* path, const void* buff, FSIZE_t ofs, UINT btw, UINT* bw);
FRESULT fvx_qcreate (const TCHAR* path, UINT btc);

// additional quick file info functions
FSIZE_t fvx_qsize (const TCHAR* path);

// additional recursive functions
FRESULT fvx_rmkdir (const TCHAR* path);
FRESULT fvx_rmkpath (const TCHAR* path);
FRESULT fvx_runlink (const TCHAR* path);

// additional wildcard based functions
FRESULT fvx_match_name(const TCHAR* path, const TCHAR* pattern);
FRESULT fvx_preaddir (DIR* dp, FILINFO* fno, const TCHAR* pattern);
FRESULT fvx_findpath (TCHAR* path, const TCHAR* pattern, BYTE mode);
FRESULT fvx_findnopath (TCHAR* path, const TCHAR* pattern);

// additional state function
bool fvx_opened(const FIL* fp);
