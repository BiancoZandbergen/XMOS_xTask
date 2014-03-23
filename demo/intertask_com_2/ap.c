/******************************************************************************
 *                                                                            *
 * File:   ap.c                                                               *
 * Author: Bianco Zandbergen <bianco [AT] zandbergen.name>                    *
 *                                                                            *
 * Demonstrating the use of inter-task communication where                    *
 * the sender and the recipient tasks reside on different kernels that        *
 * are connected to the same Communication Server (same tile).                *  
 *                                                                            *
 ******************************************************************************/
#include <stdio.h>
#include <xccompat.h>
#include "../../xtask/include/xtask.h"

#define TASK1_MAILBOX 1
#define TASK2_MAILBOX 2
#define INBOX_SIZE    4
#define OUTBOX_SIZE   4

void idle_task(void *p)
{
  while(1);
}

void task_1(void *p)
{
  struct vc_buf * buf;
  unsigned int  * data;
  
  xtask_create_mailbox(TASK1_MAILBOX, INBOX_SIZE, OUTBOX_SIZE);
  
  buf = xtask_get_outbox(TASK1_MAILBOX);
  buf->data_size = OUTBOX_SIZE;
  data = (unsigned int *) buf->data;
  
  *data = 0;

  while(1) {
    xtask_delay_ticks(200);
    xtask_send_outbox(TASK1_MAILBOX, TASK2_MAILBOX);
    (*data)++;
  }
}

void task_2(void *p)
{
  struct vc_buf * buf;
  unsigned int  * data;
  
  xtask_create_mailbox(TASK2_MAILBOX, INBOX_SIZE, OUTBOX_SIZE);
  
  while(1) {
    buf = xtask_get_inbox(TASK2_MAILBOX, ITC_LOCAL);
    data = (unsigned int *) buf->data;
    printf("%u bytes received, value: %u\n", buf->data_size, *data);
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

void init_tasks_3()
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

void start_kernel_2(chanend r, chanend w)
{
  xtask_kernel(init_tasks_3, idle_task, 100000, r, w);
}
