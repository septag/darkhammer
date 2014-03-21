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


#ifndef MODEL_IMPORT_H_
#define MODEL_IMPORT_H_

#include "dhcore/types.h"
#include "h3dimport.h"

bool_t import_model(const struct import_params* params);
bool_t import_list(const struct import_params* params);
bool_t import_listmtls(const struct import_params* params);


#endif /* MODEL_IMPORT_H_ */
