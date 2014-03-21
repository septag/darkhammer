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

#ifndef __SCRIPTLUACOMMON_H__
#define __SCRIPTLUACOMMON_H__

#include "dhcore/types.h"
#include "dhcore/mem-mgr.h"
#include "../mem-ids.h"

_EXTERN_ void* sct_alloc(size_t sz);
_EXTERN_ void sct_free(void* p);

#endif /* __SCRIPTLUACOMMON_H__ */
