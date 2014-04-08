#include "dhcore/core.h"
#include "dhcore/task-mgr.h"
#include "dhcore/hwinfo.h"

void task_run(void* params, void* result, uint thread_id, uint job_id, uint worker_idx)
{
    printf("Task-> ID:%d, Thread:%d, Worker:%d\n", job_id, thread_id, worker_idx);
    uint counter = 0;
    while (counter != 1000000000)
        counter ++;
}

void test_taskmgr()
{
    log_print(LOG_TEXT, "Initializing task-mgr ...");

    struct hwinfo info;
    hw_getinfo(&info, HWINFO_CPU);

    //tsk_zero();
    log_printf(LOG_TEXT, "Intiating %d threads ...", info.cpu_core_cnt - 1);
    tsk_initmgr(maxui(info.cpu_core_cnt - 1, 1), 0, 0, 0);

    log_print(LOG_TEXT, "Dispatching tasks #1 ...");
    uint task_id = tsk_dispatch(task_run, TSK_CONTEXT_ALL_NO_MAIN, TSK_THREADS_ALL, NULL, NULL);
    tsk_wait(task_id);
    log_print(LOG_TEXT, "tasks #1 finished");
    util_sleep(1000);
    log_print(LOG_TEXT, "Dispatching tasks #2 ...");
    uint task_id2 = tsk_dispatch(task_run, TSK_CONTEXT_ALL_NO_MAIN, TSK_THREADS_ALL, NULL, NULL);
    tsk_wait(task_id2);
    log_print(LOG_TEXT, "tasks #2 finished");

    tsk_destroy(task_id);
    tsk_destroy(task_id2);

    log_print(LOG_TEXT, "Finished, Releasing task-mgr...");
    tsk_releasemgr();
    log_print(LOG_TEXT, "done.");
}
