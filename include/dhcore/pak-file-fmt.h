/***********************************************************************************
 * Copyright (c) 2012, Sepehr Taghdisian
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 ***********************************************************************************/


#ifndef __PAKFILEFMT_H__
#define __PAKFILEFMT_H__

#include "types.h"
#include "hash.h"

#define PAK_SIGN    "HPAK"

#pragma pack(push, 1)
struct _GCCPACKED_ pak_header
{
    char sig[5];
    struct version_info v;
    uint64 items_offset;
    uint64 items_cnt;
    uint compress_mode;
};

/* pak file item, for each file in the pak I store one of these */
struct _GCCPACKED_ pak_item
{
    char filepath[DH_PATH_MAX];   /* filepath (alias) of the file for referencing */
    uint64 offset;               /* offset in the pak (in bytes) */
    uint size;                 /* actual compressed size (in bytes) */
    uint unzip_size;           /* unzipped size (in bytes) */
    hash_t hash;                 /* hash for data validity */
};
#pragma pack(pop)

#endif /*__PAKFILEFMT_H__*/
