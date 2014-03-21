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

#include "unit-test-main.h"
#include "dhcore/core.h"
#include "dhcore/json.h"
#include "dhcore/file-io.h"

void test_json()
{
    char path[DH_PATH_MAX];
    strcpy(path, DATA_DIR);
    strcat(path, "data.json");

    file_t f = fio_opendisk(path, TRUE);
    if (f != NULL)     {
        json_t j = json_parsefile(f, mem_heap());
        if (j != NULL)     {
            log_printf(LOG_TEXT, "name = %s", json_gets(json_getitem(j, "name")));
            json_t jprops = json_getitem(j, "props");
            log_printf(LOG_TEXT, "ass = %s", json_gets(json_getitem(jprops, "ass")));
            log_printf(LOG_TEXT, "age = %d", json_geti(json_getitem(jprops, "age")));
            log_printf(LOG_TEXT, "skin = %s", json_gets(json_getitem(jprops, "skin")));
            log_printf(LOG_TEXT, "cock-length = %f inch",
                json_getf(json_getitem(jprops, "cock-length")));
            log_printf(LOG_TEXT, "married = %d", json_getb(json_getitem(jprops, "married")));
        }   else    {
            err_sendtolog(FALSE);
        }

        fio_close(f);
    }
}
