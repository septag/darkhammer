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

#ifndef __PHXIMPORT_H__
#define __PHXIMPORT_H__

#include "dhcore/types.h"
#include "h3dimport.h"

bool_t import_phx_list(const struct import_params* params);
bool_t import_phx(const struct import_params* params);


#endif /* __PHXIMPORT_H__ */
