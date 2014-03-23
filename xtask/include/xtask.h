/******************************************************************************
 *                                                                            *
 * File:   xtask.h                                                            *
 * Author: Bianco Zandbergen <bianco [AT] zandbergen.name>                    *
 *                                                                            *
 * This file is part of the xTask Distributed Operating System for            *
 * the XMOS XS1 microprocessor architecture (www.xtask.org).                  *
 *                                                                            *
 * API header file.                                                           *
 * See kcalls.c for a description of the API functions.                       * 
 ******************************************************************************/
#ifndef XTASK_H
#define XTASK_H

#ifndef __XC__
#include <xccompat.h>

#define ITC_LOCAL      1
#define ITC_ANYWHERE   2

typedef void (*task_code)(void *);
typedef void (*init_code)(void);
typedef void (*hwt_code)(void *, chanend);

/* virtual channel and mailbox buffer */
struct vc_buf {
  void *data;
  unsigned int buf_size;
  unsigned int data_size;
};

/* function prototypes */
void            xtask_kernel(init_code init_threads, task_code idle_task, 
                  unsigned int timer_cycles, chanend cs_async, chanend cs_sync);
                  
int             xtask_create_init_task(task_code code, unsigned int stack_size, 
                  unsigned int priority, unsigned int tid, void *args);
                  
int             xtask_create_task(task_code code, unsigned int stack_size, 
                  unsigned int priority, unsigned int tid, void *args);

unsigned int    xtask_create_thread(hwt_code pc, unsigned int stackwords, void *args, 
                  unsigned int obj_size, unsigned int rx_buf_size, unsigned int tx_buf_size);
                  
unsigned int    xtask_create_remote_thread(unsigned int code, unsigned int stackwords, 
                  unsigned int obj_size, unsigned int rx_buf_size, unsigned int tx_buf_size);
                  
struct vc_buf * xtask_vc_get_write_buf(unsigned int handle);

struct vc_buf * xtask_vc_send(struct vc_buf *buf);

struct vc_buf * xtask_vc_receive(unsigned int handle, unsigned int min_size);

unsigned int    xtask_create_mailbox(unsigned int id, 
                  unsigned int inbox_size, unsigned int outbox_size);
struct vc_buf * xtask_get_outbox(unsigned int id);

unsigned int    xtask_send_outbox(unsigned int sender, unsigned int receiver);

struct vc_buf * xtask_get_inbox(unsigned int id, unsigned int location);

void            xtask_delay_ticks(unsigned int ticks);

#endif /* ndef __XC__ */

void xtask_comserver(chanend man_sync[], chanend man_async[], unsigned int nr_man_chan, 
                     chanend ring_in, chanend ring_out, unsigned int id);

#endif /* XTASK_H */
