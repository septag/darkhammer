/***********************************************************************************
 * Copyright (c) 2013, Sepehr Taghdisian
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

#include "util.h"

#include <stdio.h>

#include "err.h"
#include "mem-mgr.h"

char* util_readtextfile(const char* txt_filepath, struct allocator* alloc)
{
    FILE* f = fopen(txt_filepath, "rb");
    if (f == NULL)  {
        err_printf(__FILE__, __LINE__, "could not load text file '%s'", txt_filepath);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    size_t s = ftell(f);
    if (s == 0) {
        err_printf(__FILE__, __LINE__, "text file '%s' is empty", txt_filepath);
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);

    char* buffer = (char*)A_ALLOC(alloc, s + 1, 0);    
    if (buffer == NULL) {
        fclose(f);
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return NULL;
    }

    fread(buffer, s, 1, f);
    buffer[s] = 0;
    fclose(f);
    return buffer;
}
