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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "str.h"
#include "err.h"
#include "numeric.h"
#include "mem-mgr.h"

#if defined(_WIN_)
#include "win.h"
#endif

/*************************************************************************************************
 * utf8 - iso88591 encode/decoding macros
 */
#define ENCODE_85591(c) (c)
#define DECODE_85591(u16) (char)((u16) > 0xff ? '?' : c)
#define ENCODE_ASCII(c) (c)
#define DECODE_ASCII(u16) (char)((u16) > 0x7f ? '?' : c)

#if defined(_WIN_)
#define SEP_CHAR '\\'
#else
#define SEP_CHAR '/'
#endif

/*************************************************************************************************/
INLINE char* str_realloc(char* s, uint newsz)
{
    char* new_s = (char*)ALLOC(newsz, 0);
    if (new_s != NULL)  {
        memcpy(new_s, s, mem_size(s));
        FREE(s);
        return new_s;
    }
    return NULL;
}

/* */
bool_t str_isequal(const char* str1, const char* str2)
{
    return (strcmp(str1, str2) == 0);
}

bool_t str_isequal_nocase(const char* str1, const char* str2)
{
#if defined(_MSVC_)
    return (_stricmp(str1, str2) == 0);
#elif defined(_GNUC_)
    return (strcasecmp(str1, str2) == 0);
#endif
}

char* str_itos(char* instr, int n)
{
    sprintf(instr, "%d", n);
    return instr;
}

char* str_ftos(char* instr, float f)
{
    sprintf(instr, "%.3f", f);
    return instr;
}

char* str_btos(char* instr, bool_t b)
{
    if (b == TRUE)  instr[0] = '1';
    else            instr[0] = '0';
    instr[1] = 0;
    return instr;
}

int str_toint32(const char* str)
{
    return atoi(str);
}

float str_tofl32(const char* str)
{
    return (float)atof(str);
}

bool_t str_tobool(const char* str)
{
    if (str_isequal(str, "") || str_isequal(str, "0") || str_isequal_nocase(str, "false"))
        return FALSE;
    else if (str_isequal(str, "1") || str_isequal_nocase(str, "true"))
        return TRUE;
    else
        return FALSE;
}

char* str_trim(char* outstr, uint outstr_size, const char* instr, const char* trim_chars)
{
    uint i = 0;
    uint c = 0;
    char src;
    char trim;

    while ((src = instr[i++]) != 0 && c < (outstr_size-1))     {
        uint k = 0;
        bool_t found_trim = FALSE;

        while ((trim = trim_chars[k++]) != 0 && !found_trim)     {
            if (src == trim)
                found_trim = TRUE;
        }

        /* if we have a Trim char, Skip the word */
        if (!found_trim)
            outstr[c++] = src;
    }

    outstr[c] = 0;
    return outstr;
}

char* str_replace(char* str, char replace_ch, char with_ch)
{
    uint i = 0;
    char c;

    while ((c = str[i]) != 0)     {
        if (c == replace_ch)     str[i] = with_ch;
        i++;
    }
    return str;
}

char* str_widetomb(char* outstr, const wchar* instr, uint outstr_size)
{
#if defined(_WIN_)
    int outsize = WideCharToMultiByte(CP_UTF8, 0, instr, -1, NULL, 0, NULL, NULL);
    outsize = mini(outstr_size, outsize);
    outstr[0] = 0;
    WideCharToMultiByte(CP_UTF8, 0, instr, -1, outstr, outsize, NULL, NULL);
    return outstr;
#else
    size_t r = wcstombs(outstr, instr, outstr_size);
    return (r != -1) ? outstr : "";
#endif
}

wchar* str_mbtowide(wchar* outstr, const char* instr, uint outstr_size)
{
#if defined(_WIN_)
    int outsize = MultiByteToWideChar(CP_UTF8, 0, instr, -1, NULL, 0);
    outsize = mini(outstr_size, outsize);
    outstr[0] = 0;
    MultiByteToWideChar(CP_UTF8, 0, instr, -1, outstr, outsize);
    return outstr;
#else
    size_t r = mbstowcs(outstr, instr, outstr_size);
    return (r != -1) ? outstr : L"";
#endif
}

char* str_safecpy(char* outstr, uint outstr_sz, const char* instr)
{
	uint s = (uint)strlen(instr);
	if (s < outstr_sz)	{
		return strcpy(outstr, instr);
	}	else	{
		strncpy(outstr, instr, outstr_sz-1);
		outstr[outstr_sz-1] = 0;
		return outstr;
	}
}

char* str_safecat(char* outstr, uint outstr_sz, const char* instr)
{
	uint s = (uint)strlen(instr);
	uint s2 = outstr_sz - (uint)strlen(outstr);
	if (s < s2)	{
		return strcat(outstr, instr);
	}	else	{
		strncat(outstr, instr, s2 - 1);
		outstr[outstr_sz-1] = 0;
		return outstr;
	}
}

char* str_utf8_encode(const char* instr, uint instr_len, uint* out_len)
{
    uint size = instr_len;
    uint pos = instr_len;
    uint len = 0;

    char* newbuf = (char*)ALLOC(size, 0);

    while (pos > 0) {
        uint16 c = ENCODE_85591(*instr);

        size += 16; /* add 16 bytes in new buffer */
        newbuf = (char*)str_realloc(newbuf, size);

        if (c < 0x80)   {
            newbuf[len++] = (char)c;
        } else if (c > 6) {
            newbuf[len++] = (0x80 | (c & 0x3f));
        } else if (c > 12) {
            newbuf[len++] = (0xc0 | ((c >> 6) & 0x3f));
            newbuf[len++] = (0x80 | (c & 0x3f));
        } else if (c > 18) {
            newbuf[len++] = (0xe0 | ((c >> 12) & 0x3f));
            newbuf[len++] = (0xc0 | ((c >> 6) & 0x3f));
            newbuf[len++] = (0x80 | (c & 0x3f));
        }

        pos--;
        instr++;
    }

    *out_len = len;
    newbuf[len] = '\0';

    return newbuf;
}

char* str_utf8_decode(const char* instr, uint instr_len, uint* out_len)
{
    uint pos = instr_len;
    uint len = 0;
    char* newbuf = (char*)ALLOC(instr_len + 1, 0);

    while (pos > 0) {
        uint16 c = (uint8)(*instr);
        if (c >= 0xf0) { /* four bytes encoded, 21 bits */
            if(pos-4 >= 0)
                c = ((instr[0]&7)<<18) | ((instr[1]&63)<<12) | ((instr[2]&63)<<6) | (instr[3]&63);
            else
                c = '?';

            instr += 4;
            pos -= 4;
        } else if (c >= 0xe0) { /* three bytes encoded, 16 bits */
            if (pos-3 >= 0)
                c = ((instr[0]&63)<<12) | ((instr[1]&63)<<6) | (instr[2]&63);
            else
                c = '?';

            instr += 3;
            pos -= 3;
        } else if (c >= 0xc0) { /* two bytes encoded, 11 bits */
            if (pos-2 >= 0)
                c = ((instr[0]&63)<<6) | (instr[1]&63);
            else
                c = '?';

            instr += 2;
            pos -= 2;
        } else {
            instr++;
            pos--;
        }

        newbuf[len++] = DECODE_85591(c);
    }

    if (len < instr_len)
        newbuf = str_realloc(newbuf, len + 1);

    newbuf[len] = '\0';
    return newbuf;
}

void str_utf8_free(char* s)
{
    ASSERT(s);
    FREE(s);
}

void* str_toptr(const char* s)
{
    void* p;
    int r = sscanf(s, "%p", &p);
    if (r == EOF)
        p = NULL;
    return (void*)p;
}

/*************************************************************************************************/
/* path/filename helper functions */
char* path_norm(char* outpath, const char* inpath)
{
    if (inpath[0] == 0) {
        outpath[0] = 0;
        return outpath;
    }

    char tmp[DH_PATH_MAX];
#if defined(_WIN_)
    GetFullPathName(inpath, DH_PATH_MAX, tmp, NULL);
    path_towin(outpath, tmp);
    size_t sz = strlen(outpath);
    if (outpath[sz-1] == '\\')
        outpath[sz-1] = 0;
    return outpath;
#else
    realpath(inpath, tmp);
    path_tounix(outpath, tmp);
    size_t sz = strlen(outpath);
    if (outpath[sz-1] == '/')
        outpath[sz-1] = 0;
    return outpath;
#endif
}

char* path_tounix(char* outpath, const char* inpath)
{
    char tmp[DH_PATH_MAX];
    strcpy(tmp, inpath);
    str_replace(tmp, '\\', '/');
    strcpy(outpath, tmp);
    return outpath;
}

char* path_towin(char* outpath, const char* inpath)
{
    char tmp[DH_PATH_MAX];
    strcpy(tmp, inpath);
    str_replace(tmp, '/', '\\');
    strcpy(outpath, tmp);
    return outpath;
}

char* path_getdir(char* outpath, const char* inpath)
{
    /* to prevent aliasing */
    char tmp[DH_PATH_MAX];
    strcpy(tmp, inpath);

    /* Path with '/' or '\\' at the End */
    char* r = strrchr(tmp, '/');
    if (r == NULL)     r = strrchr(tmp, '\\');
    if (r != NULL)     {    strncpy(tmp, inpath, (r - tmp)); tmp[r - tmp] = 0;    }
    else               tmp[0] = 0;

    strcpy(outpath, tmp);
    return outpath;
}

char* path_getfilename(char* outpath, const char* inpath)
{
    char* r;
    char tmp[DH_PATH_MAX];
    strcpy(tmp, inpath);

    r = strrchr(tmp, '/');
    if (r == NULL)     r = strrchr(tmp, '\\');
    if (r != NULL)     strcpy(tmp, r + 1);

    /* Name only */
    r = strrchr(tmp, '.');
    if (r != NULL)     *r = 0;

    strcpy(outpath, tmp);
    return outpath;
}

char* path_getfileext(char* outpath, const char* inpath)
{
    char tmp[DH_PATH_MAX];     /* Prevent Aliasing */

    strcpy(tmp, inpath);
    char* r = strrchr(tmp, '.');
    if (r != NULL)     strcpy(tmp, r + 1);
    else               tmp[0] = 0;

    r = strchr(tmp, '/');

    strcpy(outpath, (r != NULL) ? (r + 1) : tmp);
    return outpath;
}

char* path_getfullfilename(char* outpath, const char* inpath)
{
    const char* r;
    char tmp[DH_PATH_MAX];     /* Prevent Aliasing */
    strcpy(tmp, inpath);
    r = strrchr(inpath, '/');
    if (r == NULL)     r = strrchr(inpath, '\\');
    if (r != NULL)     strcpy(tmp, r + 1);
    else               strcpy(tmp, inpath);

    strcpy(outpath, tmp);
    return outpath;
}

char* path_goup(char* outpath, const char* inpath)
{
    char tmp[DH_PATH_MAX];
    strcpy(tmp, inpath);
    size_t s = strlen(tmp);

    if (tmp[s-1] == '/' || tmp[s-1] == '\\')
        tmp[s-1] = 0;

    /* handle case when the path is like 'my/path/./' */
    if (s>3 && tmp[s-2] == '.' && (tmp[s-3] == '/' || tmp[s-3] == '\\'))
        tmp[s-3] = 0;
    /* handle case when the path is like 'my/path/.' */
    if (s>2 && tmp[s-1] == '.' && (tmp[s-2] == '/' || tmp[s-2] == '\\'))
        tmp[s-2] = 0;

    char* up = strrchr(tmp, '/');
    if (up == NULL)
        up = strrchr(tmp, '\\');

    if (up != NULL)
        *up = 0;

    strcpy(outpath, tmp);
    return outpath;
}

bool_t path_isfilevalid(const char* inpath)
{
    FILE* f = fopen(inpath, "rb");
    if (f != NULL)
        fclose(f);
    return (f != NULL);
}

char* path_join(char* outpath, const char* join0, const char* join1, ...)
{
    char tmp[DH_PATH_MAX];
    char sep[] = {SEP_CHAR, 0};

    if (join0[0] != 0)   {
        strcpy(tmp, join0);
        strcat(tmp, sep);
        strcat(tmp, join1);
    }   else    {
        strcpy(tmp, join1);
    }

    va_list args;
    va_start(args, join1);
    const char* join2;
    while ((join2 = va_arg(args, const char*)) != NULL) {
        strcat(tmp, sep);
        strcat(tmp, join2);
    }
    va_end(args);

    return strcpy(outpath, tmp);
}