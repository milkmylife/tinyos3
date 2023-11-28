
#include "tinyos.h"
#include "kernel_cc.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_proc.c"



/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{



  PCB* curproc = CURPROC;
  
  PTCB* ptcb = (PTCB*) make_ptcb(curproc);

  curproc -> thread_count += 1;

  // initilise ptcb;

  ptcb -> task = task;
  ptcb -> args = args;
  ptcb -> argl = argl;


  if(task != NULL)
  {
    TCB* new_thread = spawn_thread(curproc,ptcb,auxiliary_start_thread);  //we create a new thread
    wakeup(new_thread);
    return(Tid_t)ptcb;  
  }


	return -1;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) cur_thread();
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{

  Tid_t tidCurrent = sys_ThreadSelf();

  PTCB* ptcb = (PTCB*)tid;

  if (ptcb == NULL) {
    return -1;
  }

  if(rlist_find(&CURPROC->ptcb_list, ptcb, NULL) == NULL) {
    return -1;
  }

  if(tid == tidCurrent){
    return -1;
  }

  ptcb->refcount++;

  while(((ptcb->exited) == 0) && ((ptcb->detached) == 0)){
    kernel_wait(&ptcb->exit_cv, SCHED_USER);
  }

  ptcb->refcount--;

  if((ptcb->detached) == 1){
    return -1;
  }


  if(exitval != NULL){
    *exitval = ptcb->exitval;  
  }

  if (ptcb->refcount == 0) {
    rlist_remove(&ptcb->ptcb_list_node);
    free(ptcb);
  }
  
  return 0;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
  PTCB* ptcb = (PTCB*)tid;

  if(rlist_find(&CURPROC->ptcb_list, ptcb, NULL) == NULL) {
    return -1;
  }

  if(tid==NOTHREAD) 
    return -1;

  if(ptcb->exited == 1) 
    return -1;

  ptcb->detached=1;

  kernel_broadcast(&ptcb->exit_cv);

 // ptcb->refcount=1;

  return 0;
}
/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{

PTCB* ptcb = (PTCB*)sys_ThreadSelf();

ptcb -> exitval = exitval;
ptcb -> exited = 1;

// thread exited so we have 1 less

ptcb -> tcb -> owner_pcb -> thread_count += -1;

PCB* curproc = CURPROC;


if(curproc -> thread_count == 0)  // the process terminates when all threads have exited
{
    if(curproc->args) {
      free(curproc->args);
      curproc->args = NULL;
    }

    PCB* initpcb = get_pcb(1);  //

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
    
    if(curproc->parent != NULL) {
      /* Put me into my parent's exited list */
      rlist_push_front(& curproc->parent->exited_list, &curproc->exited_node);
      kernel_broadcast(& curproc->parent->child_exit);
    }

  

    assert(is_rlist_empty(& curproc->children_list));
    assert(is_rlist_empty(& curproc->exited_list));

    /* Disconnect my main_thread */
    curproc->main_thread = NULL;

  /* Now, mark the process as exited. */
    curproc->pstate = ZOMBIE;
}
 

  kernel_broadcast(&ptcb->exit_cv);

  /* Bye-bye cruel world :( */
  kernel_sleep(EXITED, SCHED_USER);


}

