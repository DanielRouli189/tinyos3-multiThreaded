#ifndef __KERNEL_THREADS_H
#define __KERNEL_THREADS_H


/**
  @file kernel_threads.h
  @brief TinyOS kernel: Multithreading API

  @defgroup syscalls Multithreaded System calls
  @ingroup kernel
  @brief Multithreading API

  This file contains the definition of the multithreading API,
  required in the first part of the implementation of TinyOs3
  regarding multithreaded system calls.

  @{
*/ 

#include "tinyos.h"
#include "kernel_proc.h"
#include "kernel_sched.h"
#include "kernel_streams.h"


/*******************************************
 *
 * Threads
 *
 *******************************************/


/**
  @brief The process thread control block

  An object of this type is associated to every thread. In this object
  are stored all the metadata that relate to the thread.
*/
typedef struct process_thread_control_block {
  TCB* tcb;              /**< @brief The TCB linked to this PTCB.  */
  
  Task task;             /**< @brief The task of this thread. */
  int argl;              /**< @brief The thread's argument length. */
  void* args;            /**< @brief The thread's argument string. */

  int exitval;           /**< @brief exit value. */ 
  int exited;            /**< @brief 0 if it hasn't exited, 1 otherwise. */
  int detached;          /**< @brief 0 if not detached, 1 otherwise. */
  CondVar exit_cv;       /**< @brief Thread's condition variable upon exit. */

  int refcount;          /**< @brief Counter of the number of threads the process executes. */
  rlnode ptcb_list_node; /**< @brief The rlnode variable connecting the node to the list. */
  
} PTCB;

/**
 * @brief Initialize a new process thread control block from TCB.
 * 
 * Handles the execution and the exit procedure of the
 * thread of a PTCB.
 * 
 * @param tcb 
 * @return PTCB* 
 */
PTCB* spawn_PTCB(TCB* tcb, Task task, int argl, void* args);

/** 
  @brief System call to create a new thread in the current process.

  The new thread is executed in the same process as 
  the calling thread. If this thread returns from function
  task, the return value is used as an argument to 
  `ThreadExit`.

  @param task a function to execute
  */
Tid_t sys_CreateThread(Task task, int argl, void* args);


/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf();



/**
  @brief system call to join the given thread.

  This function will wait for the thread with the given 
  tid to exit, and return its exit status in '*exitval'.
  The tid must refer to a legal thread owned by the same
  process that owns the caller. Also, the thread must 
  be undetached, or an error is returned.

  After a call to join succeeds, subsequent calls will fail
  (unless tid was re-cycled to a new thread). 

  It is possible that multiple threads try to join the
  same thread. If these threads block, then all must return the
  exit status correctly.

  @param tid the thread to join
  @param exitval a location where to store the exit value of the joined 
              thread. If NULL, the exit status is not returned.
  @returns 0 on success and -1 on error. Possible errors are:
    - there is no thread with the given tid in this process.
    - the tid corresponds to the current thread.
    - the tid corresponds to a detached thread.

  */
int sys_ThreadJoin(Tid_t tid, int* exitval);



/**
  @brief system call to detach the given thread.

  This function makes the thread tid a detached thread.
  A detached thread is not joinable (ThreadJoin returns an
  error). 

  Once a thread has exited, it cannot be detached. A thread
  can detach itself.

  @param tid the tid of the thread to detach
  @returns 0 on success, and -1 on error. Possibe errors are:
    - there is no thread with the given tid in this process.
    - the tid corresponds to an exited thread.
  */
int sys_ThreadDetach(Tid_t tid);



/**
  @brief System call to terminate the current thread.
  */
void sys_ThreadExit(int exitval);

/**
 * @brief Clean up the current process.
 * 
 * This function will be executed when the current
 * process has only one thread remaining and it wants
 * to exit. 
 * 
 * @param curproc 
 */
void cleanup_process(PCB* curproc);

/** @} */

#endif 