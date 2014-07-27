/******************************************************************************
 *                                                                            *
 * File:   kernel.c                                                           *
 * Author: Bianco Zandbergen <bianco [AT] zandbergen.name>                    *
 *                                                                            *
 * This file is part of the xTask Distributed Operating System for            *
 * the XMOS XS1 microprocessor architecture (www.xtask.org).                  *
 *                                                                            *
 * This file contains the C part of the implementation of the kernel.         *
 * More specific it contains the following functions:                         *
 *                                                                            *
 * xtask_kernel              - Initialize the kernel and initial tasks. Start *
 *                             the kernel. This function is part of the API.  *
 * xtask_kcall_handler       - Kernel call handler.                           *
 * xtask_check_delayed_tasks - Unblock tasks for which the delay has expired. *
 * xtask_get_not_chan        - get notification channel resource id.          *
 * xtask_not_handler         - handle notifications from CS.                  *
 *                                                                            *
 * kernel call implementations:                                               *
 * xtask_kcall_delay_ticks                                                    *
 * xtask_kcall_create_thread                                                  *
 * xtask_kcall_vc_receive                                                     *
 * xtask_kcall_vc_get_write_buf                                               *
 * xtask_kcall_vc_send                                                        *
 * xtask_kcall_create_mailbox                                                 *
 * xtask_kcall_create_remote_thread                                           *
 * xtask_kcall_get_outbox                                                     *
 * xtask_kcall_send_outbox                                                    *
 * xtask_kcall_get_inbox                                                      *
 * xtask_kcall_create_task                                                    *
 * xtask_kcall_exit                                                           *
 *                                                                            *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <xccompat.h>
#include "../include/kernel.h"
#include "../include/comserver.h"

/******************************************************************************
 * Function:     xtask_kernel                                                 *
 * Parameters:   init_tasks      - Function pointer to function that creates  *
 *                                 the initial tasks. This function takes     *
 *                                 void arguments.                            *
 *               idle_task       - Function pointer to a function that is     *
 *                                 run when no other tasks are ready.         *
 *                                 This function takes (void *) as argument.  *
 *                                 This function may not perform any kernel   *
 *                                 calls.                                     *
 *               tick_rate       - kernel tick rate in timer cycles (10ns)    *
 *               cs_man_async    - asynchronous channel to CS for receiving   *
 *                                 notifications.                             *
 *               cs_man_sync     - synchronous channel to CS for requests     *
 * Return:       void, this function never returns.                           *
 *               After calling the first task will run.                       *
 *                                                                            *
 *               This function initializes the kernel resources,              *
 *               creates the initial tasks and                                *
 *               starts the system by handling over the processor to          *
 *               the first task that runs. This function is part of the       *
 *               API.                                                         *
 ******************************************************************************/
#pragma stackfunction 128
void xtask_kernel(init_code    init_tasks,
                  task_code    idle_task,
                  unsigned int tick_rate,
                  chanend      cs_man_async,
                  chanend      cs_man_sync)
{
  int i;
  
  void *kstack = malloc(KSTACK_SIZE * WORD_SIZE);       // allocate kernel stack
  struct k_data *kdata = malloc(sizeof(struct k_data)); // allocate kdata struct

  for (i=0; i<8; i++) {
    kdata->sched_head[i] = NULL; // init task scheduling queues
  }

  kdata->timer_cycles = tick_rate;
  kdata->time         = 0;  
  kdata->current_task = NULL;
  kdata->delay_head   = NULL;
  kdata->block_head   = NULL;
  kdata->cs_async     = cs_man_async;
  kdata->cs_sync      = cs_man_sync;
  
  kdata->kcall_table[0]  = xtask_kcall_delay_ticks;
  kdata->kcall_table[1]  = xtask_kcall_create_thread;
  kdata->kcall_table[2]  = xtask_kcall_vc_receive;
  kdata->kcall_table[3]  = xtask_kcall_vc_get_write_buf;
  kdata->kcall_table[4]  = xtask_kcall_vc_send;
  kdata->kcall_table[5]  = xtask_kcall_create_mailbox;
  kdata->kcall_table[6]  = xtask_kcall_create_remote_thread;
  kdata->kcall_table[7]  = xtask_kcall_get_outbox;
  kdata->kcall_table[8]  = xtask_kcall_send_outbox;
  kdata->kcall_table[9]  = xtask_kcall_get_inbox;
  kdata->kcall_table[10] = xtask_kcall_create_task;
  kdata->kcall_table[11] = xtask_kcall_exit;

  _xtask_init_kdata(kstack, ((KSTACK_SIZE-2)*WORD_SIZE), kdata); // init kernel stack
  xtask_create_init_task(idle_task, 64, 7, 0, (void *)0);

  (*init_tasks)();  // create all other tasks by executing the given function 

  xtask_pick_task(kdata); // choose first task to run  
   _xtask_man_chan_setup_int(cs_man_async, (void *)kdata); // setup interrupt for 
                                                           // asynchronous (notification) channel
  _xtask_init_system();
  _xtask_restore_context(); // start first task to run!
  
}

/******************************************************************************
 * Function:      xtask_kcall_delay_ticks                                     *
 * Parameters:    callnr  - Kernel call number.                               *
 *                kdata   - Pointer to k_data structure.                      *
 *                kcall   - kernel call parameters.                           *
 *                                                                            *
 * Return:        void                                                        *
 *                                                                            *
 * Kcall params:  p0      - number of ticks to delay                          *
 *                                                                            *
 * Return params: none                                                        *
 *                                                                            *
 *                Kernel call implementation for delaying a task for a        *
 *                number of ticks.                                            *
 *                Add task to sorted delay list and pick next task to run.    *
 ******************************************************************************/
void xtask_kcall_delay_ticks(unsigned int        callnr,
                             struct k_data     * kdata, 
                             struct kcall_data * kcall)
{
  struct task_entry **xpp = &kdata->delay_head;
    
  kdata->current_task->delay = (kdata->time + kcall->p0);

  // find the right spot in the list
  while (*xpp != NULL && (*xpp)->delay <= kdata->current_task->delay) {
    xpp = &(*xpp)->next;
  }

  // add to the list
  kdata->current_task->next = *xpp;
  *xpp = kdata->current_task;

  // pick next task
  kdata->current_task = NULL;
  xtask_pick_task(kdata);
}

/******************************************************************************
 * Function:      xtask_kcall_create_thread                                   *
 * Parameters:    callnr  - Kernel call number.                               *
 *                kdata   - Pointer to k_data structure.                      *
 *                kcall   - kernel call parameters.                           *
 *                                                                            *
 * Return:        void                                                        *
 *                                                                            *
 * Kcall params:  p0      - program counter                                   *
 *                p1      - stack words                                       *
 *                p2      - args                                              *
 *                p3      - object transfer size                              *
 *                p4      - rx buffer size                                    *
 *                p5      - tx buffer size                                    *
 *                                                                            *
 * Return params: p0      - handle of new dedicated hardware thread.          *
 *                                                                            *
 *                Kernel call implementation for creating new (same tile)     *
 *                hardware thread with channel.                               *
 ******************************************************************************/
void xtask_kcall_create_thread(unsigned int        callnr,
                               struct k_data     * kdata, 
                               struct kcall_data * kcall)
{  
  struct man_msg msg;

  msg.cmd = 1;
  msg.p0  = kcall->p0;
  msg.p1  = kcall->p1;
  msg.p2  = kcall->p2;
  msg.p3  = kcall->p3;
  msg.p4  = kcall->p4;
  msg.p5  = kcall->p5;
    
  _xtask_man_sendrec(kdata->cs_sync,(void*)&msg);
  kcall->p0 = msg.p0;
}

/******************************************************************************
 * Function:      xtask_kcall_vc_receive                                      *
 * Parameters:    callnr  - Kernel call number.                               *
 *                kdata   - Pointer to k_data structure.                      *
 *                kcall   - kernel call parameters.                           *
 *                                                                            *
 * Return:        void                                                        *
 *                                                                            *
 * Kcall params:  p0      - handle to dedicated hardware thread               *
 *                p1      - minimal data to read                              *
 *                                                                            *
 * Return params: p0      - pointer to vc_buf structure with received data.   *
 *                                                                            *
 *                Kernel call implementation for receiving from a virtual     *
 *                channel to a dedicated hardware thread.                     *
 *                If no (sufficient) data is available, the task will be      *
 *                blocked.                                                    *
 ******************************************************************************/
void xtask_kcall_vc_receive(unsigned int        callnr,
                            struct k_data     * kdata, 
                            struct kcall_data * kcall)
{
  /*       
    Make a request to CS.
    If the result from CS is zero,
    no data is available, block task.
    Else return p0 which contains a pointer
    to vc_buf structure.
  */
  struct man_msg msg;
    
  msg.cmd = 2;
  msg.p0 = kcall->p0;
  msg.p1 = kcall->p1;
    
  _xtask_man_sendrec(kdata->cs_sync, (void*)&msg);
    
  if (msg.p0 == 0) { // no data available
    
    // save block data
    kdata->current_task->kcall_nr = callnr;
    kdata->current_task->kcall_params =  kcall;    

    // add process to block list
    kdata->current_task->next = kdata->block_head;
    kdata->block_head = kdata->current_task;

    // pick next task to run
    kdata->current_task = NULL;
    xtask_pick_task(kdata);
      
  } else { // new buffer with data available      
    kcall->p0 = msg.p0; // pointer to vc_buf
  }
}

/******************************************************************************
 * Function:      xtask_kcall_vc_get_write_buf                                *
 * Parameters:    callnr  - Kernel call number.                               *
 *                kdata   - Pointer to k_data structure.                      *
 *                kcall   - kernel call parameters.                           *
 *                                                                            *
 * Return:        void                                                        *
 *                                                                            *
 * Kcall params:  p0      - handle to dedicated hardware thread               *
 *                                                                            *
 * Return params: p0      - pointer to vc_buf structure with write buffer     *
 *                                                                            *
 *                Kernel call implementation for getting a virtual channel    *
 *                write buffer.                                               *
 ******************************************************************************/
void xtask_kcall_vc_get_write_buf(unsigned int        callnr,
                                  struct k_data     * kdata, 
                                  struct kcall_data * kcall)
{
  /*    
    Make a request to CS.
    Return p0 to task, contains pointer to vc_buf
  */

  struct man_msg msg;
    
  msg.cmd = 3;
  msg.p0 = kcall->p0;
    
  _xtask_man_sendrec(kdata->cs_sync, (void*)&msg);
    
  kcall->p0 = msg.p0;  
}

/******************************************************************************
 * Function:      xtask_kcall_vc_send                                         *
 * Parameters:    callnr  - Kernel call number.                               *
 *                kdata   - Pointer to k_data structure.                      *
 *                kcall   - kernel call parameters.                           *
 *                                                                            *
 * Return:        void                                                        *
 *                                                                            *
 * Kcall params:  p0      - pointer to vc_buf structure with write buffer     *
 *                                                                            *
 * Return params: p0      - pointer to vc_buf structure with write buffer     *
 *                                                                            *
 *                Kernel call implementation for sending write buffer to      *
 *                dedicated hardware thread                                   *
 ******************************************************************************/
void xtask_kcall_vc_send(unsigned int        callnr,
                         struct k_data     * kdata, 
                         struct kcall_data * kcall)
{
  /*     
    Make a request at CS.
    Return p0 to task, pointer to new vc_buf for writing
  */

  struct man_msg msg;
    
  msg.cmd = 4;
  msg.p0 = kcall->p0;
    
  _xtask_man_sendrec(kdata->cs_sync, (void*)&msg);
    
  kcall->p0 = msg.p0;      
}

/******************************************************************************
 * Function:      xtask_kcall_create_mailbox                                  *
 * Parameters:    callnr  - Kernel call number.                               *
 *                kdata   - Pointer to k_data structure.                      *
 *                kcall   - kernel call parameters.                           *
 *                                                                            *
 * Return:        void                                                        *
 *                                                                            *
 * Kcall params:  p0      - new mailbox id                                    *
 *                                                                            *
 * Return params: p0      - p0                                                *
 *                                                                            *
 *                Kernel call implementation for creating a new mailbox.      *
 ******************************************************************************/
void xtask_kcall_create_mailbox(unsigned int        callnr,
                                struct k_data     * kdata, 
                                struct kcall_data * kcall)
{
  /*
    Make a request at CS.
    Returns p0 to task, always 0, needs to be more useful
  */
  struct man_msg msg;
    
  msg.cmd = 5;
  msg.p0 = kcall->p0; // mailbox id
  msg.p1 = kdata->current_task->tid; // task id
  msg.p2 = kcall->p1; // inbox size
  msg.p3 = kcall->p2; // outbox size
    
  _xtask_man_sendrec(kdata->cs_sync, (void*)&msg);
    
  kcall->p0 = msg.p0;      
}

/******************************************************************************
 * Function:      xtask_kcall_create_remote_thread                            *
 * Parameters:    callnr  - Kernel call number.                               *
 *                kdata   - Pointer to k_data structure.                      *
 *                kcall   - kernel call parameters.                           *
 *                                                                            *
 * Return:        void                                                        *
 *                                                                            *
 * Kcall params:  p0      - function number                                   *
 *                p1      - stack words                                       *
 *                p2      - object transfer size                              *
 *                p3      - rx buffer size                                    *
 *                p4      - tx buffer size                                    *
 *                                                                            *
 * Return params: set at CS message handler                                   *
 *                                                                            *
 *                Kernel call implementation for create new (other tile!)     *
 *                hardware thread with channel.                               *
 ******************************************************************************/
void xtask_kcall_create_remote_thread(unsigned int        callnr,
                                      struct k_data     * kdata, 
                                      struct kcall_data * kcall)
{
  /*    
    Make a request at the CS.
    Block task until we receive a reply
    from the CS.
  */
  struct man_msg msg;
    
  msg.cmd = 6;
  msg.p0 = kdata->current_task->tid; // calling task id
  msg.p1 = kcall->p0; // code
  msg.p2 = kcall->p1; // nstackwords
  msg.p3 = kcall->p2; // obj size
  msg.p4 = kcall->p3; // rx buf size
  msg.p5 = kcall->p4; // tx buf size
    
  _xtask_man_send(kdata->cs_sync, (void*)&msg);
    
  /* save block data */
  kdata->current_task->kcall_nr = callnr;
  kdata->current_task->kcall_params =  kcall;    

  /* add process to block list */
  kdata->current_task->next = kdata->block_head;
  kdata->block_head = kdata->current_task;

  /* invoke scheduler */
  kdata->current_task = NULL;
  xtask_pick_task(kdata);      
}

/******************************************************************************
 * Function:      xtask_kcall_get_outbox                                      *
 * Parameters:    callnr  - Kernel call number.                               *
 *                kdata   - Pointer to k_data structure.                      *
 *                kcall   - kernel call parameters.                           *
 *                                                                            *
 * Return:        void                                                        *
 *                                                                            *
 * Kcall params:  p0      - mailbox id                                        *
 *                                                                            *
 * Return params: p0      - pointer to vc_buf structure holding outbox        *
 *                                                                            *
 *                Kernel call implementation for getting the mailbox          *
 *                outbox .                                                    *
 ******************************************************************************/
void xtask_kcall_get_outbox(unsigned int        callnr,
                            struct k_data     * kdata, 
                            struct kcall_data * kcall)
{
  /* get mail outbox
     p0 = mailbox id 
       
     Make a request at CS.
     Return p0, pointer to vc_buf to task.
  */
  struct man_msg msg;
    
  msg.cmd = 7;
  msg.p0 = kcall->p0; // mailbox id
    
  _xtask_man_sendrec(kdata->cs_sync, (void*)&msg);
    
  kcall->p0 = msg.p0;      
}

/******************************************************************************
 * Function:      xtask_kcall_send_outbox                                     *
 * Parameters:    callnr  - Kernel call number.                               *
 *                kdata   - Pointer to k_data structure.                      *
 *                kcall   - kernel call parameters.                           *
 *                                                                            *
 * Return:        void                                                        *
 *                                                                            *
 * Kcall params:  p0      - sender mailbox id                                 *
 *                p1      - recipient mailbox id                              * 
 *                                                                            *
 * Return params: set at CS message handler                                   *
 *                                                                            *
 *                Kernel call implementation for sending outbox to recipient. *
 ******************************************************************************/
void xtask_kcall_send_outbox(unsigned int        callnr,
                             struct k_data     * kdata, 
                             struct kcall_data * kcall)
{
  /*     
    Make a request at CS.
    Block task until we have received a message
    from CS that recipient has read inbox.
  */
    
  struct man_msg msg;
    
  msg.cmd = 8;
  msg.p0 = kcall->p0;
  msg.p1 = kcall->p1;
    
  _xtask_man_send(kdata->cs_sync, (void *)&msg);
    
  /* save block data */
  kdata->current_task->kcall_nr = callnr;
  kdata->current_task->kcall_params =  kcall;    

  /* add process to block list */
  kdata->current_task->next = kdata->block_head;
  kdata->block_head = kdata->current_task;

  /* invoke scheduler */
  kdata->current_task = NULL;
  xtask_pick_task(kdata);      
}

/******************************************************************************
 * Function:      xtask_kcall_get_inbox                                       *
 * Parameters:    callnr  - Kernel call number.                               *
 *                kdata   - Pointer to k_data structure.                      *
 *                kcall   - kernel call parameters.                           *
 *                                                                            *
 * Return:        void                                                        *
 *                                                                            *
 * Kcall params:  p0      - mailbox id                                        *
 *                p1      - location (LOCAL_TILE or ALL_TILES)                * 
 *                                                                            *
 * Return params: set at CS message handler                                   *
 *                                                                            *
 *                Kernel call implementation for reading inbox.               *
 ******************************************************************************/
void xtask_kcall_get_inbox(unsigned int        callnr,
                     struct k_data     * kdata, 
                     struct kcall_data * kcall)
{
  /* 
     Make a request at CS.
     Block task until response from CS.
  */
  struct man_msg msg;
    
  msg.cmd = 9;
  msg.p0 = kcall->p0; /* mailbox id */
  msg.p1 = kcall->p1; /* location: local/anywhere */
  _xtask_man_send(kdata->cs_sync, (void *)&msg);
    
  /* save block data */
  kdata->current_task->kcall_nr = callnr;
  kdata->current_task->kcall_params =  kcall;    

  /* add process to block list */
  kdata->current_task->next = kdata->block_head;
  kdata->block_head = kdata->current_task;

  /* invoke scheduler */
  kdata->current_task = NULL;
  xtask_pick_task(kdata);      
}

/******************************************************************************
 * Function:      xtask_kcall_create_task                                     *
 * Parameters:    callnr  - Kernel call number.                               *
 *                kdata   - Pointer to k_data structure.                      *
 *                kcall   - kernel call parameters.                           *
 *                                                                            *
 * Return:        void                                                        *
 *                                                                            *
 * Kcall params:  p0      - program counter                                   *
 *                p1      - stack words                                       *
 *                p2      - task priority                                     *
 *                p3      - task id                                           *
 *                p4      - args                                              *
 *                                                                            *
 * Return params: p0      - 0                                                 *
 *                                                                            *
 *                Kernel call implementation for creating a new task at       *
 *                run time.                                                   *
 ******************************************************************************/
void xtask_kcall_create_task(unsigned int        callnr,
                             struct k_data     * kdata, 
                             struct kcall_data * kcall)
{          
  // parameters
  task_code code          = (task_code) kcall->p0;
  unsigned int stack_size = kcall->p1;
  unsigned int priority   = kcall->p2;
  unsigned int tid        = kcall->p3;
  void *args              = (void *) kcall->p4;
    
  struct task_entry *pe = malloc(sizeof(struct task_entry));
    
  // allocate and initialize stack
  void *stack = malloc(stack_size * WORD_SIZE);
  void *sp = stack + ((stack_size-1)*WORD_SIZE);
  sp = _xtask_init_task_stack(sp, code, args);
    
  // initialize task entry
  pe->bottom_stack = (unsigned long*)stack;
  pe->priority     = priority;
  pe->next         = NULL;
  pe->stack_size   = stack_size;    
  pe->sp           = sp;
  pe->tid          = tid;                                                                                
    
  // we're done, schedule new task
  xtask_enqueue(kdata,pe);
    
  kcall->p0 = 0; // always return 0    
}

/******************************************************************************
 * Function:      xtask_kcall_exit                                            *
 * Parameters:    callnr  - Kernel call number.                               *
 *                kdata   - Pointer to k_data structure.                      *
 *                kcall   - kernel call parameters.                           *
 *                                                                            *
 * Return:        void                                                        *
 *                                                                            *
 * Kcall params:  p0      - exit status code (currently unused, reserved)     *
 *                                                                            *
 * Return params: none                                                        *
 *                                                                            *
 *                Kernel call implementation for exiting a task.              *
 ******************************************************************************/
void xtask_kcall_exit(unsigned int        callnr,
                      struct k_data     * kdata, 
                      struct kcall_data * kcall)
{
  /* task exit */
  free(kdata->current_task->bottom_stack);
  free(kdata->current_task);
    
  // pick next task
  kdata->current_task = NULL;
  xtask_pick_task(kdata);
    
  // need to check for other resources such as mailboxes etc    
}


/******************************************************************************
 * Function:     xtask_kcall_handler                                          *
 * Parameters:   callnr  - Kernel call number.                                *
 *               kdata   - Pointer to the kdata structure                     *
 *               kcall   - Pointer to kcall_data structure. This structure is *
 *                         provided by the calling task and contains optional *
 *                         arguments for the kernel call. It is also used to  *
 *                         pass back return values from the kernel to         *
 *                         the calling task.                                  *
 * Return:       void                                                         *
 *                                                                            *
 *               kernel call handler dispatch.                                *
 *               This function is redundant and the handlers can be called    *
 *               directly from the kernel assembly code, however this is      *
 *               not implemented yet.                                         *   
 ******************************************************************************/
void xtask_kcall_handler(unsigned int        callnr,
                         struct k_data     * kdata,
                         struct kcall_data * kcall)
{
  /*switch (callnr) {
    case 0: xtask_kcall_delay_ticks(callnr, kdata, kcall);          break;
    case 1: xtask_kcall_create_thread(callnr, kdata, kcall);        break;
    case 2: xtask_kcall_vc_receive(callnr, kdata, kcall);           break;
    case 3: xtask_kcall_vc_get_write_buf(callnr, kdata, kcall);     break;
    case 4: xtask_kcall_vc_send(callnr, kdata, kcall);              break;
    case 5: xtask_kcall_create_mailbox(callnr, kdata, kcall);       break;
    case 6: xtask_kcall_create_remote_thread(callnr, kdata, kcall); break;
    case 7: xtask_kcall_get_outbox(callnr, kdata, kcall);           break;
    case 8: xtask_kcall_send_outbox(callnr, kdata, kcall);          break;
    case 9: xtask_kcall_get_inbox(callnr, kdata, kcall);            break;
    case 10: xtask_kcall_create_task(callnr, kdata, kcall);         break;
    case 11: xtask_kcall_exit(callnr, kdata, kcall);                break;
  }*/
  
  (*kdata->kcall_table[callnr])(callnr, kdata, kcall);
}

/******************************************************************************
 * Function:     xtask_check_delayed_tasks                                    *
 * Parameters:   kdata  - pointer to kdata structure.                         *
 * Return:       void                                                         *
 *                                                                            *
 *               This function checks the list of delayed tasks and           *
 *               removes all tasks for which the delay is expired.            *
 *               These tasks are then moved to their corresponding ready      *
 *               queue. The list is sorted by expiring delays, so we can stop *
 *               searching when we find the first task for which the delay    *
 *               has not expired. This function is called on each kernel tick.*
 ******************************************************************************/
void xtask_check_delayed_tasks(struct k_data *kdata)
{
  struct task_entry *pe;

  while (kdata->delay_head != NULL && kdata->delay_head->delay == kdata->time) {
    pe = kdata->delay_head->next;
    xtask_enqueue(kdata, kdata->delay_head);
    kdata->delay_head = pe;
  }
}

/******************************************************************************
 * Function:     xtask_get_not_chan                                           *
 * Parameters:   kdata  - pointer to kdata structure.                         *
 * Return:       chanend resource id for asynchronous management channel      *
 *                                                                            *
 *               This function provides easy access to this data datastructure*
 *               for one or more asm functions. This dependency needs to be   *
 *               removed!                                                     *
 ******************************************************************************/
unsigned int xtask_get_not_chan(struct k_data *kdata)
{
  return kdata->cs_async;
}

/******************************************************************************
 * Function:     xtask_not_handler                                            *
 * Parameters:   k      - pointer to kdata structure.                         *
 * Return:       void                                                         *
 *                                                                            *
 *               This function is called when an asynchronous notification    *
 *               is received from the CS. The notification does not contain   *
 *               any further information other than that something has        *
 *               happened that interests this kernel. This function will then *
 *               ask the CS about the details and processes the reply from CS.*
 *                                                                            *
 *               To Do: move duplicate code to function.                      *
 *               For example removing tasks from block list.                  *
 *               Divide in subfunctions.                                      * 
 ******************************************************************************/
void xtask_not_handler(struct k_data *k)
{
  struct man_msg msg;

  // ask CS about the event details
  msg.cmd = 10;
  _xtask_man_sendrec(k->cs_sync,(void *)&msg);
  
  // The event details are now in msg  
  
  if (msg.cmd == 1) {
    /*  
       unblock task waiting for data from VC
       msg.p0 = handle
       msg.p1 = pointer to vc_buf
    */
    struct task_entry **xpp;
    struct task_entry *xp;
    xpp = &k->block_head;

    // find task in block list
    while (*xpp != NULL && (*xpp)->kcall_params->p0 != msg.p0) {
      xpp = &(*xpp)->next;
    }

    if (*xpp != NULL) {
      // remove task from block list
      xp = *xpp;
      *xpp = (*xpp)->next;
    } else {
      // task not found
      return;
    }

    // return pointer to vc_buf
    xp->kcall_params->p0 = msg.p1;
    
    // schedule unblocked task
    xtask_enqueue(k, xp);
    
    // add interrupted task back to scheduling queue 
    xtask_enqueue(k, k->current_task);
    
    // pick next task to run
    k->current_task = NULL;
    xtask_pick_task(k);
    
  } else if (msg.cmd == 2) {
    /*  
       Result from creating remote hardware thread
       msg.p0 = new handle
       msg.p1 = task id of requesting task
    */

    struct task_entry **xpp;
    struct task_entry *xp;
    xpp = &k->block_head;

    // find blocked task in block list
    while (*xpp != NULL && (*xpp)->tid != msg.p1) {
      xpp = &(*xpp)->next;
    }

    if (*xpp != NULL) {
      // remove task from block list
      xp = *xpp;
      *xpp = (*xpp)->next;
    } else {
      // task not found
      return;
    }
    
    // return new handle
    xp->kcall_params->p0 = msg.p0;
    
    // schedule unblocked task
    xtask_enqueue(k, xp);
    
    // move interrupted task back to scheduling queue
    xtask_enqueue(k, k->current_task);
    
    // pick next task to run
    k->current_task = NULL;
    xtask_pick_task(k);

  } else if (msg.cmd == 3) {
    /*  
       Unblock recipient task
       msg.p0 = task id
       msg.p1 = pointer to vc_buf
    */
  
    struct task_entry **xpp;
    struct task_entry *xp;
    xpp = &k->block_head;
    
    // find task in block list
    while (*xpp != NULL && (*xpp)->tid != msg.p0) {
      xpp = &(*xpp)->next;
    }

    if (*xpp != NULL) {
      // remove task from block list
      xp = *xpp;
      *xpp = (*xpp)->next;
    } else {
      // task not found
      return;
    }
    
    // return pointer to vc_buf
    xp->kcall_params->p0 = msg.p1;
    
    // schedule unblocked task
    xtask_enqueue(k, xp);
    
    // move interrupted task back to scheduling queue
    xtask_enqueue(k, k->current_task);
    
    // pick next task to run
    k->current_task = NULL;
    xtask_pick_task(k);
  
  } else if (msg.cmd == 4) {
    /*  
       Unblock sending task
       msg.p0 = task id
       msg.p1 = pointer to vc_buf
       msg.p2 = return value
    */

    struct task_entry **xpp;
    struct task_entry *xp;
    xpp = &k->block_head;

    // find task in block list
    while (*xpp != NULL && (*xpp)->tid != msg.p0) {
      xpp = &(*xpp)->next;
    }

    if (*xpp != NULL) {
      // remove task from block list
      xp = *xpp;
      *xpp = (*xpp)->next;
    } else {
      // task not found
      return;
    }
    
    // return value
    xp->kcall_params->p0 = msg.p1;
    
    // schedule unblocked task
    xtask_enqueue(k, xp);
    
    // move interrupted task back to scheduling queue
    xtask_enqueue(k, k->current_task);
    
    // pick next task to run
    k->current_task = NULL;
    xtask_pick_task(k);
  } else {
    // unknown message id received
  }
}
