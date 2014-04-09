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

#ifndef __HELPERAPP_H__
#define __HELPERAPP_H__

#include "dhapp/init-params.h"

/**
 * Sets data directory to [tutorials]/data
 */
void set_datadir();

/**
 * Sets logger output to [executable_dir]/log.txt
 */
void set_logfile();

/**
 * Loads config file from [tutorials]/data directory, if config doesn't exist
 * It Tries to load default configuration
 * Returns NULL on error
 */
init_params* load_config(const char* cfg_filename);


#endif /* __HELPERAPP_H__ */