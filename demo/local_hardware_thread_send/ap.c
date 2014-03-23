/******************************************************************************
 *                                                                            *
 * File:   ap.c                                                               *
 * Author: Bianco Zandbergen <bianco [AT] zandbergen.name>                    *
 *                                                                            *
 * Demonstrating the creation of a dedicated local hardware thread and        *
 * sending a buffer to the hardware thread.                                   *
 *                                                                            *
 ******************************************************************************/
#include <stdio.h>
#include <xccompat.h>
#include "../../xtask/include/xtask.h"

void infinite_receive(chanend c);

void idle_task(void *p)
{
  while(1);
}

void hardware_thread(void *p, chanend c)
{  
  infinite_receive(c);
}

void task_1(void *p)
{
  unsigned int handle;
  unsigned int *data;
  struct vc_buf *buf;
  unsigned int i = 0;
  
  handle = xtask_create_thread(hardware_thread, 128, (void *)0, 4, 4, 4);
  
  buf = xtask_vc_get_write_buf(handle);
  
  i = 0;
  
  while(1) {    
    buf->data_size = 4;
    data = (unsigned int *) buf->data;
    *data = i;
    buf = xtask_vc_send(buf);
    i++;
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
