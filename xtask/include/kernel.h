/******************************************************************************
 *                                                                            *
 * File:   kernel.h                                                           *
 * Author: Bianco Zandbergen <bianco [AT] zandbergen.name>                    *
 *                                                                            *
 * This file is part of the xTask Distributed Operating System for            *
 * the XMOS XS1 microprocessor architecture (www.xtask.org).                  *
 *                                                                            *
 * Kernel header file.                                                        *
 ******************************************************************************/
#ifndef KERNEL_H
#define KERNEL_H

#ifndef __XC__
#include <xccompat.h>
#endif

#define WORD_SIZE   4
#define KSTACK_SIZE 256

#define NR_KCALLS   12

typedef void (*task_code)(void *);
typedef void (*init_code)(void);
typedef void (*hwt_code)(void *, chanend);

/* kernel call parameters */
struct kcall_data {
  unsigned int p0;
  unsigned int p1;
  unsigned int p2;
  unsigned int p3;
  unsigned int p4;
  unsigned int p5;
};

struct task_entry {
  unsigned long *sp;                  /* task stack pointer */
  unsigned long *bottom_stack;        /* stack top */
  unsigned int stack_size;            /* size of stack */
  unsigned int priority;              /* priority of task: 0-7 */
  unsigned int tid;                   /* task id */
  unsigned int delay;                 /* when delayed the delay tick value is stored here */
  struct kcall_data *kcall_params;    /* when blocked, the pointer to the kcall params is stored */
  unsigned int kcall_nr;              /* and also the kernel call number is stored */
  struct task_entry *next;            /* pointer to next process for queues */
};

struct k_data {
  struct task_entry * current_task;   /* current running task */
  struct task_entry * sched_head[8];  /* heads of the ready queues */
  unsigned int timer_res;             /* timer resource handle */
  unsigned int timer_cycles;          /* number of timer cycles per tick */
  unsigned int timer_int;             /* timer value of next interrupt */
  unsigned int time;                  /* time in ticks */
  struct task_entry *delay_head;      /* head of list of delayed tasks */
  struct task_entry *block_head;      /* head of list of blocked tasks */
  unsigned int cs_async;              /* asynchronous management channel (notification) */
  unsigned int cs_sync;               /* synchronous management chnnel */
  void (*kcall_table[NR_KCALLS])(unsigned int        callnr,
                        struct k_data     * kdata, 
                        struct kcall_data * kcall);
};

/* function prototypes */
long * _xtask_init_task_stack(void *stack, task_code tc, void *args);
void   _xtask_restore_context();
void   _xtask_init_system();
void * _xtask_get_kdata();
void   xtask_enqueue(struct k_data *kdata, struct task_entry *proc);
void   xtask_pick_task(struct k_data* kdata);
void   _xtask_man_chan_setup_int(chanend c, void *env);
void   _xtask_init_kdata(void * kstack_bottom, unsigned int stack_offset, void *kdata);
int    xtask_create_init_task(task_code code, unsigned int stack_size, 
         unsigned int priority, unsigned int tid, void *args);
         
void xtask_kcall_delay_ticks          (unsigned int callnr, struct k_data * kdata, struct kcall_data * kcall);
void xtask_kcall_create_thread        (unsigned int callnr, struct k_data * kdata, struct kcall_data * kcall);
void xtask_kcall_vc_receive           (unsigned int callnr, struct k_data * kdata, struct kcall_data * kcall);
void xtask_kcall_vc_get_write_buf     (unsigned int callnr, struct k_data * kdata, struct kcall_data * kcall);
void xtask_kcall_vc_send              (unsigned int callnr, struct k_data * kdata, struct kcall_data * kcall);
void xtask_kcall_create_mailbox       (unsigned int callnr, struct k_data * kdata, struct kcall_data * kcall);
void xtask_kcall_create_remote_thread (unsigned int callnr, struct k_data * kdata, struct kcall_data * kcall);
void xtask_kcall_get_outbox           (unsigned int callnr, struct k_data * kdata, struct kcall_data * kcall);
void xtask_kcall_send_outbox          (unsigned int callnr, struct k_data * kdata, struct kcall_data * kcall);
void xtask_kcall_get_inbox            (unsigned int callnr, struct k_data * kdata, struct kcall_data * kcall);
void xtask_kcall_create_task          (unsigned int callnr, struct k_data * kdata, struct kcall_data * kcall);
void xtask_kcall_exit                 (unsigned int callnr, struct k_data * kdata, struct kcall_data * kcall);

#define ENTER_CRITICAL() __asm__ volatile("clrsr 0x02")
#define EXIT_CRITICAL()  __asm__ volatile("setsr 0x02")

#endif /* KERNEL_H */

