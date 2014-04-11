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

#if defined(_POSIXLIB_)

#include <sys/utsname.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <termios.h>
#include <pwd.h>
#include <fcntl.h>
#include <stdio.h>

#if defined(_LINUX_)
#include <sys/sendfile.h>
#elif defined(_OSX_)
#include <sys/socket.h>
  #include <sys/uio.h>
  #include <mach-o/dyld.h>
#endif

#include "mem-mgr.h"
#include "str.h"

char* util_runcmd(const char* cmd)
{
    char buff[4096];
    char* ret = NULL;
    FILE* f = popen(cmd, "r");
    if (f == NULL)
        return NULL;

    size_t offset = 0;
    fseek(f, 0, SEEK_END);
    while (!feof(f))	{
        size_t rb = fread(buff, sizeof(char), sizeof(buff), f);
        if (ret != NULL)	{
            char* tmp = ret;
            ret = ALLOC(rb + offset, 0);
            memcpy(ret, tmp, offset);
            FREE(tmp);
        }	else	{
            ret = ALLOC(rb, 0);
        }
        memcpy(ret + offset, buff, rb);
        offset += rb;
    }
    pclose(f);
    return ret;
}

char util_getch()
{
    int ch;
    struct termios oldt, newt;
    tcgetattr ( STDIN_FILENO, &oldt );
    newt = oldt;
    newt.c_lflag &= ~( ICANON | ECHO );
    newt.c_cc[VMIN] = 1;
    newt.c_cc[VTIME] = 0;

    tcsetattr ( STDIN_FILENO, TCSANOW, &newt );
    ch = getchar();
    tcsetattr ( STDIN_FILENO, TCSANOW, &oldt );
    return ch;
}

char* util_getuserdir(char* outdir)
{
    struct passwd* pw = getpwuid(getuid());
    return strcpy(outdir, pw->pw_dir);
}

char* util_gettempdir(char* outdir)
{
    return strcpy(outdir, "/tmp");
}

int util_makedir(const char* dir)
{
    return mkdir(dir, 777) == 0;
}

/* reference: http://stackoverflow.com/questions/2180079/how-can-i-copy-a-file-on-unix-using-c */
int util_copyfile(const char* dest, const char* src)
{
    int input, output;
    if ((input = open(src, O_RDONLY)) == -1)
        return FALSE;

    if ((output = open(dest, O_WRONLY | O_CREAT, O_NOFOLLOW)) == -1)		{
        close(input);
        return FALSE;
    }
    #ifdef _LINUX_
    int result = sendfile(output, input, NULL, 0) != -1;
    #else // __APPLE__
    off_t bytesCopied;
    int result = sendfile(output, input, 0, &bytesCopied, 0, 0) != -1;
    #endif
    close(input);
    close(output);
    return result;
}

int util_pathisdir(const char* path)
{
    struct stat s;
    if (stat(path, &s) == 0)
        return (s.st_mode & S_IFDIR);
    else
        return FALSE;
}

void util_sleep(uint msecs)
{
    usleep(msecs*1000);
}

int util_movefile(const char* dest, const char* src)
{
    return rename(src, dest) == 0;
}

int util_delfile(const char* filepath)
{
    return unlink(filepath);
}

#endif /* _POSIX_ */
