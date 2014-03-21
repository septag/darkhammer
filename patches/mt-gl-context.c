@line 1
#if defined(_GL_)
#include "GL/gl3w.h"
#include "glfw/glfw3-ex.h"
#include "app.h"
#endif

@line 141 (struct rs_mgr)
#if defined(_GL_)
    GLFWcontext** gl_ctxs;  /* count: load_threads_max, a context for each loader thread */
#endif

@line 264
#if defined(_GL_)
bool_t rs_gl_createctxs(uint32 thread_cnt)
{
    ASSERT(thread_cnt > 0);
    g_rs.gl_ctxs = (GLFWcontext**)ALLOC(sizeof(GLFWcontext*)*thread_cnt, MID_RES);
    if (g_rs.gl_ctxs == NULL)
        return FALSE;
    memset(g_rs.gl_ctxs, 0x00, sizeof(GLFWcontext*)*thread_cnt);

    for (uint32 i = 0; i < thread_cnt; i++) {
        g_rs.gl_ctxs[i] = glfwCreateWorkerContext((GLFWwindow*)app_get_mainwnd());
        if (g_rs.gl_ctxs[i] == NULL) 
            return FALSE;
    }
    
    return TRUE;
}

void rs_gl_destroyctxs()
{
    if (g_rs.gl_ctxs != NULL)   {
        for (uint32 i = 0; i < g_rs.load_threads_max; i++)  {
            if (g_rs.gl_ctxs[i] != NULL)
                glfwDestroyWorkerContext(g_rs.gl_ctxs[i]);
        }
        FREE(g_rs.gl_ctxs);
    }
}

/* runs in worker threads (at the beginning we dispatch this task) */
void rs_gl_init_fn(void* params, void* result, uint32 thread_id, uint32 job_id, uint32 worker_idx)
{
    ASSERT(worker_idx < g_rs.load_threads_max);

    GLFWcontext** gl_ctxs = (GLFWcontext**)params;
    ASSERT(gl_ctxs[worker_idx] != NULL);
    glfwMakeWorkerContextCurrent(gl_ctxs[worker_idx]);
}
#endif

@line 293 (rs_initmgr, inside BIT_CHECK(FLAGS, RS_FLAG_PREPARE_BGLOAD))
#if defined(_GL_)
        if (!rs_gl_createctxs(g_rs.load_threads_max))   {
            err_print(__FILE__, __LINE__, "res-mgr init failed: creating GL 'loader' context failed");
            return RET_FAIL;
        }

        /* dispatch a job to set contexts as current on each thread */
        uint32* thread_idxs = (uint32*)ALLOC(sizeof(uint32)*g_rs.load_threads_max, MID_RES);
        if (thread_idxs == NULL)    {
            err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
            return RET_OUTOFMEMORY;
        }

        for (uint32 i = 0; i < g_rs.load_threads_max; i++)
            thread_idxs[i] = i;

        uint32 initgl_job = tsk_dispatch_exclusive(rs_gl_init_fn, thread_idxs, g_rs.load_threads_max, 
            (void*)g_rs.gl_ctxs, NULL);
        FREE(thread_idxs);
        if (initgl_job != 0)    {
            tsk_wait(initgl_job);
            tsk_destroy(initgl_job);
        }
#endif

@line 324 (rs_releasemgr)
#if defined(_GL_)
    rs_gl_destroyctxs();
#endif

@line 421 (at the end of if (ptr == NULL)
#if defined(_GL_)
    else    {
        /* for gpu objects, we need to flush the GL pipeline */
        if (ldata->type == RS_RESOURCE_TEXTURE || ldata->type == RS_RESOURCE_MODEL) {
            glFlush();
        }   else if (ldata->type == RS_RESOURCE_PHXPREFAB && 
            BIT_CHECK(eng_get_params()->flags, ENG_FLAG_DEV))
        {
            glFlush();
        }
    }
#endif
