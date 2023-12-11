#include "kernel_proc.h"
#include "kernel_cc.h"
#include "kernel_streams.h"


/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{

  PCB* curproc = CURPROC;
  
  PTCB* ptcb = make_ptcb(curproc);

  if(task != NULL)
  { 
    TCB* new_thread = spawn_thread(curproc,auxiliary_start_thread);  //we create a new thread

    // initialise ptcb;
    ptcb->task = task;
    ptcb->args = args;
    ptcb->argl = argl;
    ptcb->tcb = new_thread;
    new_thread->ptcb = ptcb;
    rlist_push_back(&curproc->ptcb_list, &ptcb->ptcb_list_node);

    curproc->thread_count++;
   
    wakeup(new_thread);
    return (Tid_t)ptcb;
  }

	return NOTHREAD;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) cur_thread()->ptcb ;
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
  PTCB* ptcb = (PTCB*)tid;

  PCB* curproc = CURPROC;

  if(rlist_find(&curproc->ptcb_list, ptcb, NULL) == NULL) {
    return -1;
  }

  if(tid == sys_ThreadSelf()){
    return -1;
  }

  if((ptcb->detached) == 1){
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
  /*
  if (ptcb->refcount == 0) {
    rlist_remove(&ptcb->ptcb_list_node);
    free(ptcb);
  }
  */
  if (ptcb->refcount == 0) {
    while(!is_rlist_empty(&curproc->ptcb_list)) {
      rlnode *thing = rlist_pop_front(&curproc->ptcb_list);	
      if(thing == &ptcb->ptcb_list_node) {
        rlist_remove(&ptcb->ptcb_list_node);
        free(ptcb);
        break;
      }
      rlist_push_back(&curproc->ptcb_list, thing);
    }
    
  }

  return 0;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
	PTCB* ptcb = (PTCB*)tid;
  PCB* curproc = CURPROC;

  if(rlist_find(&curproc->ptcb_list, ptcb, NULL) == NULL) {
    return -1;
  }

  if(ptcb->exited == 1) 
    return -1;

  ptcb->detached=1;

  kernel_broadcast(&ptcb->exit_cv);

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
kernel_broadcast(&ptcb->exit_cv);

PCB* curproc = CURPROC;

curproc->thread_count--; // thread exited so we have 1 less


if(curproc -> thread_count == 0)  // the process terminates when all threads have exited
{
    if(get_pid(curproc)!=1) {

    /* Reparent any children of the exiting process to the 
       initial task */
    PCB* initpcb = get_pcb(1);
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

  assert(is_rlist_empty(& curproc->children_list));
  assert(is_rlist_empty(& curproc->exited_list));


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

  /* Disconnect my main_thread */
  curproc->main_thread = NULL;

  /* Now, mark the process as exited. */
  curproc->pstate = ZOMBIE;

  if (ptcb->refcount == 0) {
    while(!is_rlist_empty(&curproc->ptcb_list)) {
      rlnode *thing = rlist_pop_front(&curproc->ptcb_list);	
      if(thing == &ptcb->ptcb_list_node) {
        rlist_remove(&ptcb->ptcb_list_node);
        free(ptcb);
        break;
      }
      rlist_push_back(&curproc->ptcb_list, thing);
    }
    
  }
}


/* Bye-bye cruel world */
kernel_sleep(EXITED, SCHED_USER);

}