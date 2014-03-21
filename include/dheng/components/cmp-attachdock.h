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

#ifndef __CMPATTACHDOCK_H__
#define __CMPATTACHDOCK_H__

#include "../cmp-types.h"

#define CMP_ATTACHDOCK_MAX 4

enum cmp_attachdock_type
{
    CMP_ATTACHDOCK_NONE = 0,
    CMP_ATTACHDOCK_NORMAL = 1,
    CMP_ATTACHDOCK_BONE = 2
};

struct cmp_attachdock_dockdesc
{
    enum cmp_attachdock_type type;
    cmphandle_t xform_hdl;
    cmphandle_t attachment_hdl;
};

struct cmp_attachdock
{
    /* interface */
    char bindto[CMP_ATTACHDOCK_MAX][32];

    /* internal */
    uint bindto_hashes[CMP_ATTACHDOCK_MAX];
    struct cmp_attachdock_dockdesc docks[CMP_ATTACHDOCK_MAX];
};

ENGINE_API result_t cmp_attachdock_modify(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);

/* descriptors */
static const struct cmp_value cmp_attachdock_values[] = {
    {"bindto", CMP_VALUE_STRINGARRAY, offsetof(struct cmp_attachdock, bindto), 32,
    CMP_ATTACHDOCK_MAX, cmp_attachdock_modify, ""},
};
static const uint16 cmp_attachdock_type = 0xecbd;

/* */
result_t cmp_attachdock_register(struct allocator* alloc);

ENGINE_API void cmp_attachdock_unlinkall(cmphandle_t attdock_hdl);
ENGINE_API void cmp_attachdock_refresh(cmphandle_t attdock_hdl);
ENGINE_API void cmp_attachdock_clear(cmphandle_t attdock_hdl);

#endif /* __CMP-ATTACHDOCK_H__ */
