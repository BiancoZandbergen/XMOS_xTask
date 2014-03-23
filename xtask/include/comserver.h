/******************************************************************************
 *                                                                            *
 * File:   comserver.h                                                        *
 * Author: Bianco Zandbergen <bianco [AT] zandbergen.name>                    *
 *                                                                            *
 * This file is part of the xTask Distributed Operating System for            *
 * the XMOS XS1 microprocessor architecture (www.xtask.org).                  *
 *                                                                            *
 * Communication Server header file.                                          *
 ******************************************************************************/
#ifndef COMSERVER_H
#define COMSERVER_H

// inbox state flags
#define INBOX_TASK_WAITING 0x01
#define INBOX_SENDER_PEND  0x02

// pending kernel reply state flags
#define KR_FREE 0x00
#define KR_USED 0x01


// send reply back to kernel or not
#define REPLY    1
#define NO_REPLY 0

// virtual channel read buffers flags
#define CS_RD_BUF0      0x0000000001
#define CS_RD_BUF1      0x0000000002
#define TASK_RD_BUF0    0x0000000004
#define TASK_RD_BUF1    0x0000000008
#define TASK_RD_BUFS    0x000000000C
#define RD_BUF0_FIRST   0x0000000010
#define RD_BUF1_FIRST   0x0000000020
#define RD_BUF0_FILLED  0x0000000040
#define RD_BUF1_FILLED  0x0000000080
#define RD_BUFS_FILLED  0x00000000C0

// virtual channel write buffers flags
#define CS_WR_BUF0      0x0000000100
#define CS_WR_BUF1      0x0000000200
#define TASK_WR_BUF0    0x0000000400
#define TASK_WR_BUF1    0x0000000800
#define TASK_WR_BUFS    0x0000000C00
#define WR_BUF0_FIRST   0x0000001000
#define WR_BUF1_FIRST   0x0000002000
#define WR_BUF0_FILLED  0x0000004000
#define WR_BUF1_FILLED  0x0000008000
#define WR_BUFS_FILLED  0x000000C000

// virtual channel receive disabled flag
#define CS_RD_BLOCK     0x0000010000

// task blocked on virtual channel read flag
#define TASK_RD_BLOCK   0x0000020000

// management message
struct man_msg {
  unsigned int cmd;
  unsigned int p0;
  unsigned int p1;
  unsigned int p2;
  unsigned int p3;
  unsigned int p4;
  unsigned int p5;
};

#ifndef __XC__
#include <xccompat.h>
#include "../include/kernel.h"

/* look for pending senders on local CS or all CS */
#define ITC_LOCAL     1
#define ITC_ANYWHERE  2

/* pending kernel reply */
struct p_kreply {
  unsigned char state;         /* in use or not */
  struct cs_kernel *k;         /* pointer to kernel struct  that needs reply */
  struct man_msg reply;        /* the reply */
  struct p_kreply *next;       /* list pointer */
};

/* main data structure of Communication Server
   The address is pushed on the stack for easy access */
struct cs_data {
  struct cs_kernel * kernels;  /* list of all connected kernels */
  struct vchan  * vchans;      /* list of virtual channels (to hardware threads) */
  chanend ring_in;             /* ring bus input chanend */
  chanend ring_out;            /* ring bus output chanend */
  struct ring_buf *rbuf;       /* ring bus buffer */
  unsigned int id;             /* Communication Server id, must be unique */
  struct mailbox *mailboxes;   /* list of all registered mailboxes */
  struct mailbox *p_outbox;    /* list of mailboxes with pending sends (recipient not ready) */
  struct p_request *p_reqs;    /* pending ring bus replies */
  struct p_kreply k_replies[8]; /* pending kernel replies */
};

/* kernel communication information */
struct cs_kernel {
  chanend c_sync;              /* asynchronous channel (notification) */
  chanend c_async;             /* synchronous channel (management messages) */
  struct chan_event *event;    /* chanend event settings */
  struct cs_kernel *next;      /* list pointer */
  
};

/* chanend event settings */
struct chan_event {
  unsigned int res;            /* chanend resource id */
  unsigned int res_data;       /* unused currently? */
  void * vector;               /* event vector address */
  void * env;                  /* environment vector */
  void * data;
  unsigned int object_size;
  struct chan_event * next;    /* list pointer */
};

/* buffer for virtual channels and mailboxes */
struct vc_buf {
  void *data;                  /* pointer to data buffer */
  unsigned int buf_size;       /* buffer size (bytes) */
  unsigned int data_size;      /* amount of data (bytes) in buffer */
};

/* virtual channel information */
struct vchan {
  struct chan_event *event;    /* pointer to chanend event settings */
  unsigned int obj_size;       /* channel transfer object size */
  struct cs_data *csdata;      
  struct vc_buf read_bufs[2];  /* two read buffers */
  struct vc_buf write_bufs[2]; /* two write buffers */
  unsigned int state;          /* state flags, mainly buffer states */
  unsigned int handle;         /* virtual channel handle used by task */
  unsigned int min_read_size;  /* minimum amount of data to read for task */
  unsigned int thread_chanend; /* CS chanend of channel */
  unsigned int own_chanend;    /* hardware thread chanend of channel */
  struct cs_kernel *kernel;    /* kernel of task that owns this virtual channel */
  struct vchan *next;          /* list pointer */
};

/* intertask communication mailbox */
struct mailbox {
  unsigned int id;             /* mailbox id, must be unique */
  struct cs_kernel *kernel;    /* kernel of task that owns mailbox */
  unsigned int tid;            /* task id */
  struct vc_buf inbox;         /* mailbox inbox */
  struct vc_buf outbox;        /* mailbox outbox */
  unsigned int inbox_state;    /* state flags */
  unsigned int outbox_dest;    /* recipient mailbox id */
  struct mailbox *p_next;      /* list pointer for pending mailboxes list */
  struct mailbox *next;        /* list pointer for all mailboxes list */
};

/* pending ring bus reply */
struct p_request {
  struct cs_kernel *kernel;    /* kernel of task that did request */
  unsigned int tid;            /* task id */
  unsigned int msg_type;       /* ring bus message type */
  void *data;                  /* pointer to saved state */
  struct p_request *next;      /* list pointer */
};

/* ring bus buffer */
struct ring_buf {
  unsigned int cs_id;          /* Communication Server id */
  unsigned int msg_type;       /* message number */
  unsigned int status;         /* message status */
  unsigned int payload_size;   /* payload size in bytes */
  void * payload;              /* pointer to buffer */
};

// function prototypes
void test_hardware_thread(void *args, chanend c);

void         _xtask_man_sendrec(chanend c, void *msg);
void         _xtask_man_send(chanend c, void *msg);
void         _xtask_man_chan_vec();
void         _xtask_vc_vect();
void         _xtask_set_chan_event(void *chan_event);
chanend      _xtask_get_chanend();
void         _xtask_set_chanend_dest(chanend chan, chanend dest);
void         _xtask_set_cs_data(void *data);
unsigned int _xtask_create_thread(void *pc, void *sp, void *args, chanend c);
void         _xtask_send_man_msg(chanend c, void *msg);
void         _xtask_chan_enable_events(chanend c);
void         _xtask_ring_send(void *csdata);
void         _xtask_notify_kernel(chanend ce);
void         _xtask_ring_vec();

void         test_hardware_thread(void *args, chanend c);

struct mailbox   * xtask_get_mailbox(struct cs_data *csdata, unsigned int id);
struct p_kreply  * xtask_get_free_kreply(struct cs_data *csdata);
struct p_kreply  * xtask_get_kreply(struct cs_data *csdata, struct cs_kernel *k);
struct p_request * xtask_get_free_p_request(struct cs_data *csdata);

#endif /* ndef __XC__ */

#endif /* COMSERVER_H */
