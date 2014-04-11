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

#ifndef __MT_H__
#define __MT_H__

#include "types.h"
#include "allocator.h"
#include "core-api.h"

#if defined(_POSIXLIB_)
#include <pthread.h>
#else
#include "win.h"
#endif

/**
 * @defgroup mt Multi-threading
 */

/*************************************************************************************************
 * Atomics
 */

 /**
 * Atomic operations (wrappers)\n
 * Atomic types should be defined with @e volatile keyword\n
 * here are the atomic macros:\n
 * @b MT_ATOMIC_CAS(dest_ptr, cmp_value, swap_value): compare-and-swap, returns original value\n
 * @b MT_ATOMIC_SET(dest_ptr, value): set atomic value\n
 * @b MT_ATOMIC_INCR(dest_ptr) : increment atomic, returns new value\n
 * @b MT_ATOMIC_DECR(dest_ptr): decrements atomic, returns new value\n
 * @b MT_ATOMIC_CASTPTR(dest, cmp_ptr, new_ptr): compare-and-swap pointer, returns original value\n
 * @b MT_ATOMIC_SETPTR(dest, ptr): set atomic pointer\n
 * @ingroup mt
 */
 
#if defined(_X86) || defined(_ARM_)
typedef long volatile atom_t;
#elif defined(_X64_)
typedef int64 volatile atom_t;
#endif

#if defined(_WIN_)
/* windows specific */
#include "win.h"
#define MT_ATOMIC_CAS(dest, cmp_value, swap_value)     \
    InterlockedCompareExchange(&(dest), (swap_value), (cmp_value))
#define MT_ATOMIC_CAS64(dest, cmp_value, swap_value)   \
    InterlockedCompareExchange64(&(dest), (swap_value), (cmp_value))
#define MT_ATOMIC_SET(dest, value)     \
    InterlockedExchange(&(dest), (value))
#define MT_ATOMIC_SET64(dest, value)   \
    InterlockedExchange64(&(dest), (value))
#define MT_ATOMIC_INCR(dest)   \
    InterlockedIncrement(&(dest))
#define MT_ATOMIC_DECR(dest_ptr)   \
    InterlockedDecrement(&(dest))
#define MT_ATOMIC_CASTPTR(dest, cmp, new_ptr)  \
    InterlockedCompareExchangePointer(&(dest), (new_ptr), (cmp))
#define MT_ATOMIC_SETPTR(dest, ptr)   \
    InterlockedExchangePointer(&(dest), (ptr))
#elif defined(_POSIXLIB_)
/* unix/linux specific */
#define MT_ATOMIC_CAS(dest, cmp_value, swap_value)     \
    __sync_val_compare_and_swap(&(dest), (cmp_value), (swap_value))
#define MT_ATOMIC_CAS64 MT_ATOMIC_CAS
#define MT_ATOMIC_SET(dest, value)     \
    __sync_lock_test_and_set(&(dest), (value))
#define MT_ATOMIC_SET64 MT_ATOMIC_SET
#define MT_ATOMIC_INCR(dest)   \
    __sync_add_and_fetch(&(dest), 1)
#define MT_ATOMIC_DECR(dest)   \
    __sync_sub_and_fetch(&(dest), 1)
#define MT_ATOMIC_CASTPTR(dest, cmp, new_ptr)  \
	__sync_val_compare_and_swap(&(dest), (cmp), (new_ptr))
#define MT_ATOMIC_SETPTR(dest, ptr) \
	__sync_lock_test_and_set(&(dest), (ptr))
#endif

/**
 * @ingroup mt
 */
#define MT_TIMEOUT_INFINITE INVALID_INDEX

/*************************************************************************************************
 * Mutex
 */
#if defined(_POSIXLIB_)
typedef pthread_mutex_t     mt_mutex;
#elif defined(_WIN_)
typedef CRITICAL_SECTION    mt_mutex;
#endif

/**
 * @fn mt_mutex_init(mt_mutex* m)
 * Create mutex object
 * @ingroup mt
 */

/**
 * @fn void mt_mutex_release(mt_mutex* m)
 * Destroy mutex object
 * @ingroup mt
 */

/**
 * @fn void mt_mutex_lock(mt_mutex* m)
 * Locks mutex. blocks the program if it's mutex is already locked
 * @ingroup mt
 */

/**
 * @fn void mt_mutex_unlock(mt_mutex* m)
 * Unlocks mutex and remove program block
 * @ingroup mt
 */

/**
 * @fn int mt_mutex_try(mt_mutex* m)
 * Try locking the mutex object, passes if mutex is already locked, if not, then locks the mutex
 * @return TRUE if mutex is successfully locked, FALSE if mutex was already locked before
 * @ingroup mt
 */

#if defined(_POSIXLIB_)
INLINE void mt_mutex_init(mt_mutex* m)  {   pthread_mutex_init(m, NULL);    }
INLINE void mt_mutex_release(mt_mutex* m) {   pthread_mutex_destroy(m);   }
INLINE void mt_mutex_lock(mt_mutex* m)    {   pthread_mutex_lock(m);  }
INLINE void mt_mutex_unlock(mt_mutex* m)  {   pthread_mutex_unlock(m);    }
INLINE int mt_mutex_try(mt_mutex* m) {return (pthread_mutex_trylock(m) == 0);}
#elif defined(_WIN_)
INLINE void mt_mutex_init(mt_mutex* m)  {   InitializeCriticalSection(m);   }
INLINE void mt_mutex_release(mt_mutex* m) {   DeleteCriticalSection(m);   }
INLINE void mt_mutex_lock(mt_mutex* m)    {   EnterCriticalSection(m);    }
INLINE void mt_mutex_unlock(mt_mutex* m)  {   LeaveCriticalSection(m);    }
INLINE int mt_mutex_try(mt_mutex* m) { return TryEnterCriticalSection(m); }
#endif


/*************************************************************************************************
 * Threads
 */

/**
 * Event
 * @ingroup mt
 */
struct mt_event_data;
typedef struct mt_event_data* mt_event;

/**
 * Thread
 * @ingroup mt
 */
struct mt_thread_data;
typedef struct mt_thread_data* mt_thread;

/**
 * Thread kernel callback function, first paramter must be casted to thread
 * @see thread
 * @ingroup mt
 */
typedef result_t (*pfn_mt_thread_kernel)(mt_thread thread);

/**
 * Thread init callback function, first paramter must be casted to thread
 * @see thread
 * @ingroup mt
 */
typedef result_t (*pfn_mt_thread_init)(mt_thread thread);

/**
 * Thread release callback function, first paramter must be casted to thread
 * @see thread
 * @ingroup mt
 */
typedef void (*pfn_mt_thread_release)(mt_thread thread);

/**
 * @ingroup mt
 */
enum mt_thread_priority
{
    MT_THREAD_NORMAL = 0,
    MT_THREAD_HIGH,
    MT_THREAD_LOW
};


/**
 * Reponse for event @e wait functions
 * @ingroup mt
 */
enum mt_event_response
{
    MT_EVENT_OK = 0,    /**< Wait is continued without problems */
    MT_EVENT_TIMEOUT,   /**< Waiting timed out */
    MT_EVENT_ERROR  /**< Events raised errors during wait */
};

/**
 * Creates an event, Events can have multiple signals, which you can wait and trigger them
 * @ingroup mt
 */
CORE_API mt_event mt_event_create(struct allocator* alloc);

/**
 * Destroys an event
 * @ingroup mt
 */
CORE_API void mt_event_destroy(mt_event e);

/**
 * Adds signal to an event, signals are identified by their IDs and can be used for syncing threads
 * @return ID of the created signal, or zero if error occured
 * @ingroup mt
 */
CORE_API uint mt_event_addsignal(mt_event e);

/**
 * Waits on a specific signal of the event and blocks the execution of the calling thread,
 * until @e mt_event_trigger is called upon that signal, or timeout is reached
 * @param e Event that owns the signal
 * @param signal_id ID of the signal returned by @e mt_event_addsignal
 * @param timeout Timeout that blocking should continue, set to @b MT_TIMEOUT_INFINITE to wait
 * infinitely
 * @return Event's wait response
 * @ingroup mt
 * @see mt_event_addsignal
 * @see mt_event_trigger
 */
CORE_API enum mt_event_response mt_event_wait(mt_event e, uint signal_id, uint timeout);

/**
 * Waits on all signals of the event and blocks the execution of the calling thread,
 * until @e mt_event_trigger is called upon all signals, or timeout is reached
 * @param e Event that owns the signal
 * @param timeout Timeout that blocking should continue, set to @b MT_TIMEOUT_INFINITE to wait
 * infinitely
 * @return Event's wait response
 * @ingroup mt
 * @see mt_event_addsignal
 * @see mt_event_trigger
 */
CORE_API enum mt_event_response mt_event_waitforall(mt_event e, uint timeout);

/**
 * Triggers the event signal, that causes the current thread to continue (if blocked)
 * @param e Event that owns the signal
 * @param signal_id Signal that we want to trigger it
 * @see mt_event_wait
 * @see mt_event_waitforall
 * @ingroup mt
 */
CORE_API void mt_event_trigger(mt_event e, uint signal_id);

/**
 * Creates a thread and start running it's kernel immediately
 * @param kernel_fn kernel function callback which executes in an infinite loop
 * @param init_fn initialize function (OPTIONAL), that implements thread initialzation code
 * @param release_fn release function (OPTIONAL), that implmenets thread release code
 * @param pr thread priority (see enum thread_priority)
 * @param local_mem_sz Local @e data memory size (freelist allocator), in bytes
 * @param tmp_mem_sz @e Temp memory size (stack allocator), in bytes
 * @param param1 Custom parameter that is saved in the thread for program use
 * @param param2 Custom parameter that is saved in the thread for program use
 * @ingroup mt
 */
CORE_API mt_thread mt_thread_create(
    pfn_mt_thread_kernel kernel_fn, pfn_mt_thread_init init_fn, pfn_mt_thread_release release_fn,
    enum mt_thread_priority level, size_t local_mem_sz, size_t tmp_mem_sz,
    void* param1, void* param2);

/**
 * Destroys a thread. blocks the program until thread is stopped and exited
 * @ingroup mt
 */
CORE_API void mt_thread_destroy(mt_thread thread);

/**
 * Stops execution of kernel code, the thread does not exit, but waits for user to resume the thread
 * @see mt_thread_resume
 * @ingroup mt
 */
CORE_API void mt_thread_pause(mt_thread thread);

/**
 * Resume execution of kernel code
 * @see mt_thread_pause
 * @ingroup mt
 */
CORE_API void mt_thread_resume(mt_thread thread);

/**
 * Stop execution of thread, this function does not wait for thread to finish work,
 * just sends stop message
 * @ingroup mt
 */
CORE_API void mt_thread_stop(mt_thread thread);

/**
 * Returns param1 of the thread
 * @ingroup mt
 * @see mt_thread_create
 */
CORE_API void* mt_thread_getparam1(mt_thread thread);

/**
 * Returns param2 of the thread
 * @ingroup mt
 * @see mt_thread_create
 */
CORE_API void* mt_thread_getparam2(mt_thread thread);

/**
 * Returns thread internal ID
 * @ingroup mt
 */
CORE_API uint mt_thread_getid(mt_thread thread);

/**
 * Returns thread's local allocator
 * @ingroup mt
 */
CORE_API struct allocator* mt_thread_getlocalalloc(mt_thread thread);

/**
 * Returns thread's temp allocator
 * @ingroup mt
 */
CORE_API struct allocator* mt_thread_gettmpalloc(mt_thread thread);

/**
 * Resets thread's stack allocator to offset zero
 * @ingroup mt
 */
CORE_API void mt_thread_resettmpalloc(mt_thread thread);


#endif /*__MT_H__*/

