/******************************************************************************
 *                                                                            *
 * File:   ap.c                                                               *
 * Author: Bianco Zandbergen <bianco [AT] zandbergen.name>                    *
 *                                                                            *
 * Demonstrating the use of the task delay API function.                      * 
 *                                                                            *
 ******************************************************************************/
#include <xccompat.h>
#include "../../xtask/include/xtask.h"
#include "../common/led.h"

void idle_task(void *p)
{
  while(1);
}

void task_1(void *p)
{
  while(1) {
    xtask_delay_ticks(250);
    set_leds();
    xtask_delay_ticks(250);
    clr_leds();
  }
}

void task_2(void *p)
{
  while(1) {
    xtask_delay_ticks(25); 
  }
}

void init_tasks_1()
{
  xtask_create_init_task(task_1, 512, 1, 1, (void *)0);
  
}

void init_tasks_2()
{
  xtask_create_init_task(task_2, 512, 1, 2, (void *)0);
}

void start_kernel_0(chanend r, chanend w)
{
  xtask_kernel(init_tasks_1, idle_task, 100000, r, w);
}

void start_kernel_1(chanend r, chanend w)
{
  xtask_kernel(init_tasks_2, idle_task, 100000, r, w);
}
