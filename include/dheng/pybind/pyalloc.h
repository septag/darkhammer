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

#ifndef __PYALLOC_H__
#define __PYALLOC_H__

#if defined(calloc)
#undef calloc
#endif

#if defined(free)
#undef free
#endif

#if defined(malloc)
#undef malloc
#endif

/* fwd declare */
void* py_alignedalloc(size_t s);
void py_alignedfree(void* ptr);

DEF_ALLOC void* malloc(size_t s)
{
    return py_alignedalloc(s);
}

DEF_ALLOC void free(void* ptr)
{
    py_alignedfree(ptr);
}

DEF_ALLOC void* calloc(size_t num, size_t s)
{
    void* p = py_alignedalloc(s*num);
    if (p == NULL)
        return NULL;
    memset(p, 0x00, s*num);
    return p;
}

#endif /* __PYALLOC_H__ */
