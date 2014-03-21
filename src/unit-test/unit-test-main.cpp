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

#include <stdio.h>
#include <stdlib.h>
#include "dhcore/core.h"
#include "unit-test-main.h"

typedef void (*pfn_test)();

struct unit_test_desc
{
    pfn_test    test_fn;
    const char* name;
    const char* desc;
};

static const unit_test_desc g_tests[] = {
    {test_heap, "heap", "heap allocation"},
    {test_freelist, "freelist", "freelist allocator"},
    {test_json, "json", "json"},
    {test_mempool, "pool", "pool allocator"},
    {test_thread, "thread", "basic threads"},
    {test_taskmgr, "task-mgr", "Task manager"}
    /*, {test_efsw, "watcher", "filesystem monitoring"}*/
};

int show_help()
{
    printf("choose unit test: \n");
    uint test_cnt = sizeof(g_tests)/sizeof(struct unit_test_desc);

    for (uint i = 0; i < test_cnt; i++)  {
        printf("%d- %s (%s)\n", i, g_tests[i].desc, g_tests[i].name);
    }
    printf("q- quit\n");

    char r = util_getch();
    if (r == 'q')   {
        return -1;
    }   else    {
        char rs[2];
        rs[0] = (char)r;
        rs[1] = 0;
        return atoi(rs);
    }
}

int parse_cmd(const char* arg)
{
    uint test_cnt = sizeof(g_tests)/sizeof(struct unit_test_desc);
    for (uint i = 0; i < test_cnt; i++)     {
        if (str_isequal_nocase(arg, g_tests[i].name))   {
            return i;
        }
    }
    return -1;
}

int main(int argc, char** argv)
{
    int r = -1;

    if (IS_FAIL(core_init(CORE_INIT_ALL)))     {
        printf("core init error.\n");
        return -1;
    }

    char log_filepath[DH_PATH_MAX];
    path_join(log_filepath, util_getexedir(log_filepath), "log.txt", NULL);

    log_outputconsole(TRUE);
#if !defined(_DEBUG_)
    log_outputfile(TRUE, log_filepath);
#endif

    printf("hammer engine unit test\n");

    if (r == -1)    {
        uint test_cnt = sizeof(g_tests)/sizeof(struct unit_test_desc);
        const char* first_arg = argv[1];
        if (first_arg != NULL)
            r = parse_cmd(first_arg);
        if (r == -1 || r >= (int)test_cnt)    {
            r = show_help();
        }
    }

    if (r == -1)    {
        core_release(TRUE);
        return 0;
    }   else    {
        g_tests[r].test_fn();
    }

    puts("");
    core_release(TRUE);
    return 1;
}
