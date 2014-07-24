/******************************************************************************
 *                                                                            *
 * File:   ap.c                                                               *
 * Author: Bianco Zandbergen <bianco [AT] zandbergen.name>                    *
 *                                                                            *
 * Demonstrating the creation of a new task at run time by another task.      * 
 *                                                                            *
 ******************************************************************************/
#include <stdio.h>
#include <xccompat.h>
#include "../../xtask/include/xtask.h"

void idle_task(void *p)
{
  while(1);
}

void task_2(void *p)
{ 
  while(1) {
    printf("task 2\n");
    xtask_delay_ticks(1000);
  }
}

void task_1(void *p)
{
  xtask_create_task(task_2, 512, 1, 2, (void *)0);
  
  while(1) {
    printf("task 1\n");
    xtask_delay_ticks(1000);
  }
}



void init_tasks_1()
{
  xtask_create_init_task(task_1, 512, 1, 1, (void *)0); 
}

void init_tasks_2()
{
  
}

void start_kernel_0(chanend r, chanend w)
{
  xtask_kernel(init_tasks_1, idle_task, 100000, r, w);
}

void start_kernel_1(chanend r, chanend w)
{
  xtask_kernel(init_tasks_2, idle_task, 100000, r, w);
}
