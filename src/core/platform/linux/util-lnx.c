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

#include "util.h"

#if defined(_LINUX_)

#include <sys/utsname.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <stdio.h>

#include "str.h"

char* util_getexedir(char* outpath)
{
    char tmp[32];
    sprintf(tmp, "/proc/%d/exe", getpid());
    size_t bytes = readlink(tmp, outpath, DH_PATH_MAX-1);
    outpath[bytes] = 0;
    return path_getdir(outpath, outpath);
}


#endif /* _LINUX_ */
