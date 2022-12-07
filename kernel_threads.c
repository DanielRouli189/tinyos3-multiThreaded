
#include "tinyos.h"
#include "kernel_proc.h"
#include "kernel_sched.h"
#include "kernel_streams.h"
#include "kernel_cc.h"
#include "kernel_threads.h"
#include "unit_testing.h"
#include "util.h"

/**
 * @brief Initialize a new process thread control block from TCB.
 * 
 * Handles the execution and the exit procedure of the
 * thread of a PTCB.
 * 
 * @param tcb 
 * @return PTCB* 
 */
PTCB* spawn_PTCB(TCB* tcb, Task task, int argl, void* args){
  PTCB* ptcb = (PTCB*)xmalloc(sizeof(PTCB));  /* memory allocation for PTCB    */
  
  ASSERT(ptcb != NULL);
  ptcb->tcb = tcb;
  tcb->ptcb = ptcb;
  
  ptcb->task = task;
  ptcb->argl = argl;
  ptcb->args = args;

  ptcb->exitval = -1;
  ptcb->exited = 0;
  ptcb->detached = 0;
  ptcb->exit_cv = COND_INIT;
  
  ptcb->refcount = 0;

  rlnode_init(& ptcb->ptcb_list_node, tcb->ptcb);

  
  return ptcb;
}

/**
 * @brief Start a new thread.
 * 
 */
void start_thread() 
{
  int exitval;
  /* pointing to thread's ptcb */
  PTCB* ptcb = (PTCB*) sys_ThreadSelf();

  Task call = ptcb->task;
  int argl = ptcb->argl;
  void* args = ptcb->args;

  exitval = call(argl,args);
  sys_ThreadExit(exitval);
}


/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
  if(task == NULL) return -1;

  /*spawn a new thread*/
  PCB* pcb = CURPROC;
  TCB* new_thread = spawn_thread(pcb, start_thread); 
  PTCB* ptcb = spawn_PTCB(new_thread, task, argl, args);
  
  rlist_push_back(& pcb->ptcb_list, rlnode_init(&ptcb->ptcb_list_node, ptcb));
  pcb->thread_count++;

  wakeup(new_thread);

  return (Tid_t) ptcb;
}


/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) cur_thread()->ptcb;
}


/**
  @brief Join the given thread.

  This function will wait for the thread with the given 
  tid to exit, and return its exit status in '*exitval'.
  @param tid
  @param exitval

  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{

  /*Find the PTCB from the given Tid*/
  PTCB* ptcb = (PTCB*) tid;
  PCB* pcb = CURPROC;

  /* checks if PTCB exists in CURPROC list */
  if(rlist_find(& pcb->ptcb_list, ptcb, NULL) == NULL) return -1;

  /* checks if PTCB is detached */
  if(ptcb->detached) return -1;

  /* checks if thread tries to join itself or if Tid is invalid */
  if(ptcb == cur_thread()->ptcb || tid == NOTHREAD) return -1;
  
  /*increase refcount*/
  ptcb->refcount++;
  
  /*till the thread becomes exited or detached*/ 
  while(!ptcb->exited && !ptcb->detached) {
    kernel_wait(& ptcb->exit_cv, SCHED_USER);
  }
  
  ptcb->refcount--;

  /*if thread becomes detached it can no longer join */ 
  if(ptcb->detached) return -1;

  if(exitval != NULL) *exitval = ptcb->exitval;

  /* if there are not any threads, remove list_node and free PTCB*/
  if (ptcb->refcount == 0){
    rlist_remove(& ptcb->ptcb_list_node);
    free(ptcb);
  }

  return 0;
  
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
  /*find PTCB from the given Tid*/
  PTCB* ptcb = (PTCB*) tid;
  PCB* pcb = CURPROC;

  /* check if PTCB exists in CURPROC list */
  if(rlist_find(& pcb->ptcb_list, ptcb, NULL) == NULL) return -1;
  

  /* check if ptcb is exited or if Tid is invalid */
  if (ptcb->exited || tid == NOTHREAD) return -1;

  /*if everything is okay, detach the thread */
  if(!ptcb->detached){    
    ptcb->detached = 1;

    /* wake everyone up */
    kernel_broadcast(& ptcb->exit_cv);
  }

  return 0;
}


/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{
  PCB* curproc = CURPROC;
  TCB* curThread = cur_thread();
  PTCB* ptcb = curThread->ptcb;
  
  ptcb->exitval = exitval;
  ptcb->exited = 1;

  kernel_broadcast(& ptcb->exit_cv);

  curproc->thread_count--;

  if(curproc->thread_count == 0)
    cleanup_process(curproc);
  
  kernel_sleep(EXITED, SCHED_USER);
}

/**
 * @brief Clean up the current process.
 * 
 * @param curproc 
 */
void cleanup_process(PCB* curproc) {
  /* Reparent any children of the exiting process to the 
     initial task
  */
  PCB* initpcb = get_pcb(1);
  if(get_pid(curproc) != 1) {
    while(!is_rlist_empty(& curproc->children_list)) {
      rlnode* child = rlist_pop_front(& curproc->children_list);
      child->pcb->parent = initpcb;
      rlist_push_front(& initpcb->children_list, child);
    }

    /* Add exited children to the initial task's exited list 
      and signal the initial task */
    if(!is_rlist_empty(& curproc->exited_list)) {
      rlist_append(& initpcb->exited_list, &curproc->exited_list);
      kernel_broadcast(& initpcb->child_exit);
    }

    /* Put me into my parent's exited list */
    rlist_push_front(& curproc->parent->exited_list, &curproc->exited_node);
    kernel_broadcast(& curproc->parent->child_exit);
  }

  ASSERT(is_rlist_empty(& curproc->children_list));
  ASSERT(is_rlist_empty(& curproc->exited_list));

  /* 
    Do all the other cleanup we want here, close files etc. 
  */

  /* Release the args data */
  if(curproc->args) {
    free(curproc->args);
    curproc->args = NULL;
  }

  /* Clean up FIDT */
  for(int i=0;i<MAX_FILEID;i++) {
    if(curproc->FIDT[i] != NULL) {
      FCB_decref(curproc->FIDT[i]);
      curproc->FIDT[i] = NULL;
    }
  }

 /* clean up the entire list */
  while(!is_rlist_empty(&(curproc->ptcb_list)))
    rlist_pop_front(&(curproc->ptcb_list));


  /* Disconnect my main_thread */
  curproc->main_thread = NULL;

  /* Now, mark the process as exited. */
  curproc->pstate = ZOMBIE;

  /* Bye-bye cruel world */
}