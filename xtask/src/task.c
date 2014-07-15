/******************************************************************************
 *                                                                            *
 * File:   task.c                                                             *
 * Author: Bianco Zandbergen <bianco [AT] zandbergen.name>                    *
 *                                                                            *
 * This file is part of the xTask Distributed Operating System for            *
 * the XMOS XS1 microprocessor architecture (www.xtask.org).                  *
 *                                                                            *
 * This file contains some task related functions.                            *
 * More specific it contains the following functions:                         *
 *                                                                            *
 * xtask_create_init_task  - create initial task                              *
 * xtask_enqueue           - add task to scheduling queues                    *
 * xtask_pick_task         - pick next task to run (scheduler)                *
 *                                                                            *
 ******************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include "../include/kernel.h"

 /*****************************************************************************
 * Function:     xtask_create_init_task                                       *
 * Parameters:   code         - Pointer to the function that the task will    *
 *                              execute. This function must return void and   *
 *                              accept a void * as argument                   *
 *               stack_size   - Stack size in words                           *
 *               priority     - Task priority, 0-6, lower number is higher    *
 *                              priority                                      *
 *               tid          - Task id, must be unique                       *
 *               args         - Pointer to argument buffer (or use the        *
 *                              pointer value itself as argument)             *
 * Return:       Currently always returns 0                                   *
 *                                                                            *
 *               Create initial task (before the operating system starts).    *
 ******************************************************************************/  
int xtask_create_init_task(task_code    code, 
                           unsigned int stack_size, 
                           unsigned int priority, 
                           unsigned int tid, 
                           void *       args)
{
  struct task_entry *pe = malloc(sizeof(struct task_entry));

  // allocate stack memory
  void *stack = malloc(stack_size * WORD_SIZE);
  
  // calculate top of stack
  void *sp = stack + ((stack_size-1)*WORD_SIZE);

  // bottom of stack is same value as returned by malloc
  pe->bottom_stack = (unsigned long*)stack;
  
  pe->priority = priority;
  
  // we don't know the next task in the scheduling queue
  pe->next = NULL;
    
  pe->stack_size = stack_size;
  
  sp = _xtask_init_task_stack(sp, code, args);
  
  pe->sp = sp;
  pe->tid = tid;
  
  // add task to the right scheduling queue
  xtask_enqueue(_xtask_get_kdata(),pe);

  return 0;
}

 /*****************************************************************************
 * Function:     xtask_enqueue                                                *
 * Parameters:   kdata  - pointer to kdata structure                          *
 *               proc   - pointer to the task_entry structure of the task     *
 *                        that needs to be added to the scheduling queue      * 
 * Return:       void                                                         *
 *                                                                            *
 *               Add a task to one of the scheduling queues based on the      *
 *               priority of the task.                                        *
 ******************************************************************************/
void xtask_enqueue(struct k_data *kdata, struct task_entry *proc)
{
  struct task_entry **xpp;
  
  xpp = &kdata->sched_head[proc->priority];
  
  while (*xpp != NULL) {
    xpp = &(*xpp)->next;
  }

  proc->next = *xpp;
  *xpp = proc;
}

 /*****************************************************************************
 * Function:     xtask_pick_task                                              *
 * Parameters:   kdata  - pointer to kdata structure                          *
 * Return:       void                                                         *
 *                                                                            *
 *               Pick the next task to run. The scheduling algorithm is       *
 *               multi-level queues scheduling. If any task is still          *
 *               scheduled, it must be added to the scheduling queues         *
 *               before calling this function.                                *
 ******************************************************************************/
void xtask_pick_task(struct k_data* kdata)
{
  int i;

  /*if (kdata->current_task != NULL) {
    printf("xtask_pick_task: current_task != NULL!\n");
  }*/

  for (i = 0; i < 8; i++) {
    if (kdata->sched_head[i] != NULL) {
      // we found a non-empty scheduling queue
      kdata->current_task  = kdata->sched_head[i];       // first task in queue is next task
      kdata->sched_head[i] = kdata->sched_head[i]->next; // remove from queue
      kdata->current_task->next = NULL;                  // reset list pointer
      break;
    }
  }
}


