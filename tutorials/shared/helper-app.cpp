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

#include "dhcore/core.h"
#include "dhcore/file-io.h"

#include "dheng/app.h"
#include "dheng/engine.h"

#include "helper-app.h"

void set_datadir()
{
    const char* share_dir = eng_get_sharedir();
    ASSERT(share_dir);

    char data_path[DH_PATH_MAX];
    path_join(data_path, share_dir, "tutorials", "data", NULL);

    /* Set the second parameter (monitor) to TRUE, for hot-reloading of assets within that directory */
    fio_addvdir(data_path, FALSE);
}

void set_logfile()
{
    char logfile[DH_PATH_MAX];
    path_join(logfile, util_getexedir(logfile), "log.txt", NULL);

    log_outputfile(TRUE, logfile);
}

init_params* load_config(const char* cfg_filename)
{
    char datadir[DH_PATH_MAX];
    path_join(datadir, util_getexedir(datadir), "data", cfg_filename, NULL);

    init_params* params = app_load_config(cfg_filename);
    if (params == NULL) {
        err_sendtolog(TRUE);
        return app_defaultconfig();
    }
    return params;
}



