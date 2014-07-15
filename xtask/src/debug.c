/* various (temporary) debug functions */

#include <stdio.h>
#include <stdlib.h>
#include <xccompat.h>
#include "../include/kernel.h"
#include "../include/man_chan.h"
#include "../include/comserver.h"

void dump_k_replies(struct cs_data *csdata, unsigned int id)
{
  int i;
  
  printf("Dump k_replies [%u]: ", id);
  
  for (i= 0; i< 8; i++) {
    if (csdata->k_replies[i].state & KR_USED) {
      printf("[%p %u(0x%x)] ", csdata->k_replies[i].k, csdata->k_replies[i].state, csdata->k_replies[i].state);
    }
  }
  
  printf("\n");
}

void dump_kernels(struct cs_kernel *head)
{
  printf("*head: %p\n", head);
  while (head != NULL) {
    printf("kernel struct: current %p next: %p &next: %p\n", head, head->next, &head->next);
    head = head->next;
  }
}
/**
 * Debug function to print all tasks
 * in the scheduling queues
 */  
void dump_queues(struct k_data *kdata)
{
  int i;
  struct task_entry *p;

  //printf("dump_queues\n");  

  if (kdata->current_task == NULL) {
    printf("current_task: none\n");
  }  else {
    printf("current_task: %u\n",kdata->current_task->tid);
  }

  for (i=0; i<8; i++) {
    p = kdata->sched_head[i];

    //if (p == NULL) printf("Q: %d empty\n",i);

    while(p != NULL) {
      printf("Q: %d, tid: %u prio: %u ss: %u\n",i,p->tid,p->priority,p->stack_size);
      p = p->next;  
    }

  }
  printf("--\n");
}
