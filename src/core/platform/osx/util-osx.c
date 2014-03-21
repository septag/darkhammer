/***********************************************************************************
 * Copyright (c) 2013, Davide Bacchet
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

#if defined(_OSX_)

#include <sys/socket.h>
#include <sys/uio.h>
#include <mach-o/dyld.h>

#include "str.h"

char* util_getexedir(char* outpath)
{
    uint32_t tmpsize = DH_PATH_MAX;
    _NSGetExecutablePath(outpath, &tmpsize);
    return path_getdir(outpath, outpath);
}

#endif /* _OSX_ */
