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

#ifndef __CMPATTACHMENT_H__
#define __CMPATTACHMENT_H__

#include "../cmp-types.h"

/* descriptors */
struct cmp_attachment
{
    /* interface */
    char attachto[32];
    uint dock_slot;

    /* internal */
    uint target_obj_id;
    cmphandle_t dock_hdl;
};

ENGINE_API result_t cmp_attachment_modifyattachto(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);
ENGINE_API result_t cmp_attachment_modifydockslot(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);

static const struct cmp_value cmp_attachment_values[] = {
    {"attachto", CMP_VALUE_STRING, offsetof(struct cmp_attachment, attachto), 32, 1,
    cmp_attachment_modifyattachto, ""},
    {"dock-slot", CMP_VALUE_UINT, offsetof(struct cmp_attachment, dock_slot), sizeof(uint), 1,
    cmp_attachment_modifydockslot, ""}
};
static const uint16 cmp_attachment_type = 0xabd4;

/* */
result_t cmp_attachment_register(struct allocator* alloc);

ENGINE_API void cmp_attachment_unlink(cmphandle_t attach_hdl);
ENGINE_API void cmp_attachment_attach(struct cmp_obj* obj, struct cmp_obj* target, const char* dockname);
ENGINE_API void cmp_attachment_detach(struct cmp_obj* obj);

#endif /* __CMP-ATTACHMENT_H__ */
