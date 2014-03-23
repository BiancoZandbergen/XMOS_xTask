/******************************************************************************
 *                                                                            *
 * File:   ap.c                                                               *
 * Author: Bianco Zandbergen <bianco [AT] zandbergen.name>                    *
 *                                                                            *
 * Demonstrating the creation of a dedicated local hardware thread and        *
 * receiving a completely filled buffer from the hardware thread.             *  
 *                                                                            *
 ******************************************************************************/
#include <stdio.h>
#include <xccompat.h>
#include "../../xtask/include/xtask.h"

void infinite_send(chanend c);

void idle_task(void *p)
{
  while(1);
}

void hardware_thread(void *p, chanend c)
{  
  infinite_send(c);  
}

void task_1(void *p)
{
  unsigned int handle;
  unsigned int *data;
  struct vc_buf *buf;
  
  handle = xtask_create_thread(hardware_thread, 128, (void *)0, 4, 4, 4);
  
  while (1) {
    buf = xtask_vc_receive(handle, 0);
    data = (unsigned int *) buf->data;
    printf("received from hardware thread: %u\n", *data);  
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
