/******************************************************************************
 *                                                                            *
 * File:   comserver.c                                                        *
 * Author: Bianco Zandbergen <bianco [AT] zandbergen.name>                    *
 *                                                                            *
 * This file is part of the xTask Distributed Operating System for            *
 * the XMOS XS1 microprocessor architecture (www.xtask.org).                  *
 *                                                                            *
 * This file contains the C part of the implementation of the                 *
 * Communication Server. More specific it contains the following functions:   *
 *                                                                            *
 * xtask_comserver                 - initialise and start CS                  *
 * xtask_vc_send_buf               - send a buffer to hardware thread         *
 * xtask_process_man_msg           - process received management message      *
 * xtask_cs_get_rd_ptr             - get new read pointer to store next       *
 *                                   object received from hardware thread     *
 * xtask_cs_check_rd_blocked_tasks - unblock tasks that were blocked on a     *
 *                                   read on virtual channel (from hardware   *
 *                                   thread)                                  *
 * xtask_process_ring_msg          - process received ring message            *
 * xtask_get_mailbox               - get mailbox by id                        *
 * xtask_get_free_kreply           - get free pending kernel reply            *                                
 * xtask_get_kreply                - get pending kernel reply for given kernel*                                           
 * xtask_get_free_p_request        - get free pending ring bus reply          *                                          
 *                                                                            *
 ******************************************************************************/  

#include <stdlib.h>
#include <stdio.h>
#include <xccompat.h>
#include <string.h>
#include "../include/comserver.h"

/******************************************************************************
 * Function:     xtask_comserver                                              *
 * Parameters:   man_sync[]    - Array of sync management channel chanends.   *
 *               man_async[]   - Array of async management channel chanends.  *
 *               nr_man_chan   - Number of management channels (pairs).       *
 *                               Equals the number of kernels connected.      *
 *               ring_in       - chanend for ingoing ring bus messages.       *
 *               ring_out      - chanend for outgoing ring bus messages.      *
 * Return:       does not return, waits for event                             *
 *                                                                            *
 *               Initialises and starts the Communication Server.             *
 *               Allocates and initialises data structures.                   *
 *               Initialises management and ring buffer channels.             *
 *               Starts server by waiting for an event on chanends.           *
 *               This function is part of the API.                            * 
 ******************************************************************************/
#pragma stackfunction 128
void xtask_comserver(chanend      man_sync[], 
                     chanend      man_async[], 
                     unsigned int nr_man_chan, 
                     chanend      ring_in, 
                     chanend      ring_out, 
                     unsigned int id)
{
  int i;
  
  // allocate the main data structure of CS
  struct cs_data * csdata = malloc(sizeof(struct cs_data));

  csdata->kernels   = NULL;
  csdata->vchans    = NULL;
  csdata->mailboxes = NULL;
  csdata->p_reqs    = NULL;
  csdata->p_outbox  = NULL;
  csdata->id        = id;
  
  csdata->ring = (!ring_in || !ring_out) ? 0 : 1; // has ring bus?

  if (csdata->ring) {
    // ring_buf contains the buffer information for ring bus messages  
    csdata->rbuf          = malloc(sizeof(struct ring_buf));
    csdata->rbuf->payload = malloc(512);
    
    csdata->ring_in       = ring_in;
    csdata->ring_out      = ring_out;
  
    // chan_event contains the information needed by event vectors that execute upon receiving data
    // this chan_event is for receiving messages from the ring bus
    struct chan_event *ev = malloc(sizeof(struct chan_event));
    ev->res    = ring_in;                     // chanend belonging to this chan_event
    ev->vector = (void *)  _xtask_ring_vec;   // vector that is executed when data is available
    ev->env    = (void *) csdata;             // address of csdata as environment vector
    _xtask_set_chan_event((void *)ev);        // configure chanend and enable events on chanend
  }

  // initialize all management channels
  // for each management channel pair we allocate a kernel structure
  // containing the information to communicate with this kernel
  for (i = 0; i < nr_man_chan; i++) {
    struct cs_kernel *temp = malloc(sizeof(struct cs_kernel));
        
    temp->c_sync             = man_sync[i];
    temp->c_async            = man_async[i];
    temp->event              = (struct chan_event *) malloc(sizeof(struct chan_event));
    temp->event->res         = temp->c_sync;
    temp->event->data        = (struct man_msg *) (malloc(sizeof(struct man_msg)));
    temp->event->object_size = sizeof(struct man_msg);
    temp->event->vector      = (void *)_xtask_man_chan_vec;
    temp->event->env         = (void *)temp->event;
    
    _xtask_set_chan_event((void *)temp->event); // configure chanend and enable events on chanend

    temp->next = csdata->kernels; // add kernel structure to list
    csdata->kernels = temp;
  }
  
  // free all pending kernel replies 
  for (i=0; i<8; i++) {
    //csdata->k_replies[i].state &= ~(KR_USED);
    csdata->k_replies[i].state = 0;
    csdata->k_replies[i].k     = 0;
  }

  _xtask_set_cs_data((void *)csdata); // push csdata address on stack
  __asm__ volatile ("waiteu");        // start server by waiting for requests from kernels
}

/******************************************************************************
 * Function:     xtask_vc_send_buf                                            *
 * Parameters:   vc      - Pointer to vchan structure                         *
 *               bufnr   - buffer number (0 or 1)                             *
 * Return:       does not return, waits for event                             *
 *                                                                            *
 *               Send a buffer with objects to a dedicated hardware thread    *
 *               through a channel. The object size must be a multiple of     *
 *               4 bytes.                                                     *
 ******************************************************************************/
void xtask_vc_send_buf(struct vchan * vc, 
                       unsigned int   bufnr)
{
  unsigned int  obj_size  = vc->obj_size;
  struct vc_buf *vcbuf    = &vc->write_bufs[bufnr];
  unsigned int  data_size = vcbuf->data_size;
  unsigned int  chan_end  = vc->own_chanend;
  unsigned int  nr_words;
  
  void *current_obj;
  void *temp_obj;
  int i, j;

  current_obj = vcbuf->data;

  nr_words = obj_size / 4; // number of words per object

  for (i=0; i<data_size; i+=obj_size) { // for each object
   
    // send object synchronised to hardware thread
    __asm__ volatile ("outct res[%0], 0x01"::"r"(chan_end));
    __asm__ volatile ("chkct res[%0], 0x01"::"r"(chan_end));
    
    current_obj += obj_size;
    temp_obj = current_obj;
    
    for (j=0; j<nr_words; j++) {
      temp_obj -= 4; // send highest word in memory of object first
      __asm__ volatile ("out res[%0], %1"::"r"(chan_end),"r"(*((unsigned int*)temp_obj)));
    }   

    __asm__ volatile ("outct res[%0], 0x01"::"r"(chan_end));
    __asm__ volatile ("chkct res[%0], 0x01"::"r"(chan_end));
  }
}

/******************************************************************************
 * Function:     xtask_process_man_msg                                        *
 * Parameters:   csdata  - Pointer to cs_data structure.                      *
 *               evt     - Pointer to chan_event structure.                   *
 * Return:       whether CS should send a reply back to the kernel            *
 *                                                                            *
 *               Process a request received throught a management channel     *
 *               from a kernel.                                               *
 *                                                                            * 
 *               This function should be divided in subfunctions to           *
 *               increase readability.                                        *
 ******************************************************************************/ 
unsigned int xtask_process_man_msg(struct cs_data *    csdata,
                                   struct chan_event * evt)
{
  if (((struct man_msg*)evt->data)->cmd == 1) {
    /* 
       create new hardware thread with channel
       p0 = pc
       p1 = stackwords
       p2 = args
       p3 = object size
       p4 = rx buf size
       p5 = tx buf size 
    */
  
    chanend a = _xtask_get_chanend();
    chanend b = _xtask_get_chanend();
    _xtask_set_chanend_dest(a,b);
    _xtask_set_chanend_dest(b,a);
    
    // create new hardware thread
    unsigned int * new_stack = (unsigned int *)malloc(((struct man_msg*)evt->data)->p1 * WORD_SIZE);
    void *         new_sp    = (void*) (new_stack + (((struct man_msg*)evt->data)->p1 - 1));
    unsigned int   handler   = _xtask_create_thread((void*)((struct man_msg*)evt->data)->p0, 
                                                          (void*)new_sp, 
                                                          (void*)((struct man_msg*)evt->data)->p2, 
                                                           b);
                                                           
    // allocate and initialise new chan_event structure for hardware thread
    struct chan_event *new_ce = (struct chan_event*) malloc(sizeof(struct chan_event));
    new_ce->res = a;
    new_ce->vector = (void *) _xtask_vc_vect;
    
    ((struct man_msg*)evt->data)->p0 = handler; // return handle to kernel
    ((struct man_msg*)evt->data)->p1 = a;       // return CS chanend to hardware thread, seems to be not used by kernel

    // allocate and initialise new vchan structure for hardware thread
    struct vchan *new_vchan   = malloc(sizeof(struct vchan));
    new_vchan->own_chanend    = a;
    new_vchan->thread_chanend = b;
    new_vchan->handle         = handler;
    new_vchan->event          = new_ce;
    new_vchan->state          = 0;
    new_vchan->read_bufs[0].data       =  malloc(((struct man_msg*)evt->data)->p4);
    new_vchan->read_bufs[0].buf_size   =  ((struct man_msg*)evt->data)->p4;
    new_vchan->read_bufs[0].data_size  =  0;
    new_vchan->read_bufs[1].data       =  malloc(((struct man_msg*)evt->data)->p4);
    new_vchan->read_bufs[1].buf_size   =  ((struct man_msg*)evt->data)->p4;
    new_vchan->read_bufs[1].data_size  =  0;
    new_vchan->write_bufs[0].data      =  malloc(((struct man_msg*)evt->data)->p5);
    new_vchan->write_bufs[0].buf_size  =  ((struct man_msg*)evt->data)->p5;
    new_vchan->write_bufs[0].data_size =  0;
    new_vchan->write_bufs[1].data      =  malloc(((struct man_msg*)evt->data)->p5);
    new_vchan->write_bufs[1].buf_size  =  ((struct man_msg*)evt->data)->p5;
    new_vchan->write_bufs[1].data_size =  0;
    new_vchan->obj_size                =  ((struct man_msg*)evt->data)->p3;
    new_vchan->csdata                  =  csdata;

    struct cs_kernel *temp_k = csdata->kernels;
    
    // find kernel that made the request (and has the task that made the request)
    while (temp_k != NULL) {
      if (temp_k->c_sync == evt->res) {
        break;
      }

      temp_k = temp_k->next;
    }

    new_vchan->kernel = temp_k; // save a pointer to that kernel

    // add vchan to list of virtual channels
    new_vchan->next = csdata->vchans;
    csdata->vchans = new_vchan;

    new_ce->env = new_vchan; // address of vchan structure, environment vector for hardware thread receive vector
    _xtask_set_chan_event((void*)new_ce); // start receive data from hardware thread
    return REPLY; // send a reply back to kernel
    
  } else if (((struct man_msg*)evt->data)->cmd == 2) {
    /*
       Task requests a (optionally partially) filled read buffer
       from virtual channel to hardware thread.
       p0 = handle
       p1 = min read size
    */
    
    // find the right virtual channel in the list by the given handle
    struct vchan *temp_vchan = csdata->vchans;
    while (temp_vchan != NULL) {
      if (temp_vchan->handle == ((struct man_msg*)evt->data)->p0) {
        break;
      }

      temp_vchan = temp_vchan->next;
    }

    if (temp_vchan != NULL) {

      temp_vchan->min_read_size = ((struct man_msg*)evt->data)->p1; 
  
      // task made a new read request, so set the data size of the
      // previously held buffer to zero.
      if (temp_vchan->state & TASK_RD_BUF0) {
        temp_vchan->read_bufs[0].data_size = 0;
      } else if (temp_vchan->state & TASK_RD_BUF1) {
        temp_vchan->read_bufs[1].data_size = 0;
      }
      
      // new read request, clear flags that indicate that the task is
      // using the read buffers.
      temp_vchan->state &= ~(TASK_RD_BUFS);
      
      // find a buffer to return, if there is any of course
      if (temp_vchan->state & RD_BUFS_FILLED) {
        // at least one of both buffers are completely filled
        
        if ( (temp_vchan->state & RD_BUFS_FILLED) == RD_BUFS_FILLED) {
          // both buffers are completely filled, which one to pick?
          if (temp_vchan->state & RD_BUF0_FIRST) {  // return buf 0, it was filled first
            temp_vchan->state &= ~(RD_BUF0_FIRST);  // clear buffer fill order flag
            temp_vchan->state |= TASK_RD_BUF0;      // task uses buf now
            temp_vchan->state &= ~(RD_BUF0_FILLED); // clear buffer filled flag
            ((struct man_msg*)evt->data)->p0 = (unsigned int)&temp_vchan->read_bufs[0];
            
          } else if (temp_vchan->state & RD_BUF1_FIRST) { // return buf 1, it was filled first
            temp_vchan->state &= ~(RD_BUF1_FIRST);
            temp_vchan->state |= TASK_RD_BUF1;
            temp_vchan->state &= ~(RD_BUF1_FILLED);
            ((struct man_msg*)evt->data)->p0 = (unsigned int)&temp_vchan->read_bufs[1];
          }
        } else if (temp_vchan->state & RD_BUF0_FILLED) {
          // only buf 0 is filled, return this one
          temp_vchan->state |= TASK_RD_BUF0;
          temp_vchan->state &= ~(RD_BUF0_FILLED);
          ((struct man_msg*)evt->data)->p0 = (unsigned int)&temp_vchan->read_bufs[0];
          
        } else if (temp_vchan->state & RD_BUF1_FILLED) {
          // only buf 1 is filled, return this one
          temp_vchan->state |= TASK_RD_BUF1;
          temp_vchan->state &= ~(RD_BUF1_FILLED);
          ((struct man_msg*)evt->data)->p0 = (unsigned int)&temp_vchan->read_bufs[1]; 
        }

      } else {
        // no completely filled buffer was available but maybe we can find
        // a partly filled buffer that meets the tasks requirement
        // for the minimal amount of data. If minimal amount is 0, we only
        // want completely filled buffers
        
        if (temp_vchan->state & CS_RD_BUF0 &&
          temp_vchan->min_read_size > 0 &&
          temp_vchan->read_bufs[0].data_size >= temp_vchan->min_read_size) {
          // CS has partially filled buf 0 and it meets the amount requirement, return this buffer
          temp_vchan->state &= ~(CS_RD_BUF0);
          temp_vchan->state |= TASK_RD_BUF0;
          ((struct man_msg*)evt->data)->p0 = (unsigned int)&temp_vchan->read_bufs[0];

        } else if (temp_vchan->state & CS_RD_BUF0 &&
          temp_vchan->min_read_size > 0 &&
          temp_vchan->read_bufs[0].data_size >= temp_vchan->min_read_size) {
          // CS has partially filled buf 0 and it meets the amount requirement, return this buffer
          temp_vchan->state &= ~(CS_RD_BUF1);
          temp_vchan->state |= TASK_RD_BUF1;
          ((struct man_msg*)evt->data)->p0 = (unsigned int)&temp_vchan->read_bufs[1];

        } else {
          ((struct man_msg*)evt->data)->p0 = 0; // send null pointer to kernel as buffer pointer
          temp_vchan->state |= TASK_RD_BLOCK;   // indicate that the task will be blocked by the kernel
        }
      }

      // check if channel events needs to be reenabled
      // channel events are disabled when there is no
      // buffer available because the task is using them
      // or has not yet read them.
      if (temp_vchan->state & CS_RD_BLOCK) {
        if (( !(temp_vchan->state & TASK_RD_BUF0) && !(temp_vchan->state & RD_BUF0_FILLED)) ||
            ( !(temp_vchan->state & TASK_RD_BUF1) && !(temp_vchan->state & RD_BUF1_FILLED))) {
          _xtask_chan_enable_events(temp_vchan->event->res);
        }
      }
    } else {
      // virtual channel not found, should not get here
    }
    
    return REPLY; // return buffer to read or null pointer to kernel
    
  } else if (((struct man_msg*)evt->data)->cmd == 3) {
    /*
       Task requests a buffer to fill and
       send to hardware thread.
       p0 = handle
    */
    
    struct vchan *vc = csdata->vchans;
    
    // find the virtual channel in the list by the given handle
    while (vc != NULL) {
      if (vc->handle == ((struct man_msg*)evt->data)->p0) {
        break;
      }

      vc = vc->next;
    }
    
    // return one of the available buffers
    // we actually assume that both buffers are free and thus also
    // not in use by CS to transfer to a hardware thread
    // this because this request should be made before the first transfer
    // and should only be made once because requesting a transfer
    // will return a new buffer
    // we still check the flags just to be sure
    if (! (vc->state & TASK_WR_BUF0)) {
      vc->state |= TASK_WR_BUF0;
      vc->write_bufs[0].data_size = 0;
      ((struct man_msg*)evt->data)->p0 = (unsigned int)&vc->write_bufs[0];
    } else if (! (vc->state & TASK_WR_BUF1)) {
      vc->state |= TASK_WR_BUF1;
      vc->write_bufs[1].data_size = 0;
      ((struct man_msg*)evt->data)->p0 = (unsigned int)&vc->write_bufs[1];
    } else {
      ((struct man_msg*)evt->data)->p0 = 0;
      // both not available, should not get here
    }
    
    return REPLY; // return the buffer pointer to the kernel

  } else if (((struct man_msg*)evt->data)->cmd == 4) {
    /* 
       Task requests to transfer a buffer to a hardware thread
       p0 = pointer to vc_buf structure 
    */
    unsigned int bufnr;
    struct vchan *vc = csdata->vchans;
    
    // find the virtual channel by the given vc_buf structure
    while (vc != NULL) {
      if (&vc->write_bufs[0] == (struct vc_buf *)((struct man_msg*)evt->data)->p0) {
        bufnr = 0;
        break;
      } else if (&vc->write_bufs[1] == (struct vc_buf *)((struct man_msg*)evt->data)->p0) {
        bufnr = 1;
        break;
      }
      vc = vc->next;
    }

    // return new buffer before start transmission to hardware task
    if (bufnr == 0) {
      
      vc->state &= ~(TASK_WR_BUF0); // clear buffer 0 in use by task

      // return new buffer or return null pointer if there is no free buffer
      if (! (vc->state & TASK_WR_BUF1)) {
        ((struct man_msg*)evt->data)->p0 = (unsigned int)&vc->write_bufs[1];
      } else {
        ((struct man_msg*)evt->data)->p0 = 0;
      }
    } else if (bufnr == 1) {
      
      vc->state &= ~(TASK_WR_BUF1); // clear buffer 1 in use by task

      // return new buffer or return null pointer if there is no free buffer
      if (! (vc->state & TASK_WR_BUF0)) {
        ((struct man_msg*)evt->data)->p0 = (unsigned int)&vc->write_bufs[0];
      } else {
        ((struct man_msg*)evt->data)->p0 = 0;
      }
    }
    
    _xtask_man_send(evt->res, evt->data); // send reply to kernel
                                          // with new buffer

    xtask_vc_send_buf(vc, bufnr); // send the buffer to the hardware thread
    return NO_REPLY; // already have sent the reply
    
  } else if (((struct man_msg*)evt->data)->cmd == 5) {
    /*
       Task requests to register a new mailbox.
       p0 = mailbox id
       p1 = task id
       p2 = inbox size
       p3 = outbox size
    */
    // allocate a new mailbox structure
    struct mailbox *reg = malloc(sizeof(struct mailbox));
    reg->id  = ((struct man_msg*)evt->data)->p0;
    reg->tid = ((struct man_msg*)evt->data)->p1;

    // find the kernel from which the request originates
    struct cs_kernel *temp_k = csdata->kernels;
    
    while (temp_k != NULL) {
      if (temp_k->c_sync == evt->res) {
        break;
      }

      temp_k = temp_k->next;
    }

    if (temp_k == NULL) {
      // should not get here
    } else {
      reg->kernel = temp_k;
    }

    // create inbox
    reg->inbox.buf_size  = ((struct man_msg*)evt->data)->p2;
    reg->inbox.data_size = 0;
    reg->inbox.data      = malloc(reg->inbox.buf_size);
    reg->inbox_state     = 0; 
    
    // create outbox
    reg->outbox.buf_size  = ((struct man_msg*)evt->data)->p3;
    reg->outbox.data_size = 0;
    reg->outbox.data      = malloc(reg->outbox.buf_size); 

    // add to the front of list with mailboxes
    reg->next = csdata->mailboxes;
    csdata->mailboxes = reg;

    ((struct man_msg*)evt->data)->p0 = 1; // returns 1 for now
  
    return REPLY;
  
  } else if (((struct man_msg*)evt->data)->cmd == 6) {
    /*
       Task requests to create a new remote hardware thread
       p0 = code (task number!, not a pointer to a function)
       p1 = stackwords
       p2 = args
       p3 = object size
       p4 = rx buf size
       p5 = tx buf size
    */
    struct p_request **prp;
    
    if (!csdata->ring) {
      struct p_kreply *kr;
      
      // we don't have a ring bus, cannot create remote dedicated hardware thread
      // add kernel reply to queue and notify kernel
      kr = xtask_get_free_kreply(csdata);

      if (kr != NULL) {
        
        struct cs_kernel *temp_k = csdata->kernels;
    
        // find the kernel that has the pending reply
        while (temp_k != NULL) {
          if (temp_k->c_sync == evt->res) {
            break;
          }
    
          temp_k = temp_k->next;
        }
        
        kr->state    |= KR_USED;
        kr->k         = temp_k;
        kr->reply.cmd = 2;
        kr->reply.p1  = ((struct man_msg*)evt->data)->p0;
        kr->reply.p2 = 1; // return value, failure
          
        _xtask_notify_kernel(temp_k->c_async);
      }
      
      return NO_REPLY;
    }
    
    // create a new virtual channel
    struct vchan *new_vchan = malloc(sizeof(struct vchan));
    new_vchan->own_chanend  = _xtask_get_chanend();
    new_vchan->state        = 0;
    new_vchan->read_bufs[0].data       = malloc(((struct man_msg*)evt->data)->p4);
    new_vchan->read_bufs[0].buf_size   = ((struct man_msg*)evt->data)->p4;
    new_vchan->read_bufs[0].data_size  = 0;
    new_vchan->read_bufs[1].data       = malloc(((struct man_msg*)evt->data)->p4);
    new_vchan->read_bufs[1].buf_size   = ((struct man_msg*)evt->data)->p4;
    new_vchan->read_bufs[1].data_size  = 0;
    new_vchan->write_bufs[0].data      = malloc(((struct man_msg*)evt->data)->p5);
    new_vchan->write_bufs[0].buf_size  = ((struct man_msg*)evt->data)->p5;
    new_vchan->write_bufs[0].data_size = 0;
    new_vchan->write_bufs[1].data      = malloc(((struct man_msg*)evt->data)->p5);
    new_vchan->write_bufs[1].buf_size  = ((struct man_msg*)evt->data)->p5;
    new_vchan->write_bufs[1].data_size = 0;
    new_vchan->obj_size                = ((struct man_msg*)evt->data)->p3;
    new_vchan->min_read_size           = 0;
    new_vchan->csdata                  = csdata;
  
    // prepare ring bus message
    csdata->rbuf->cs_id    = csdata->id;
    csdata->rbuf->msg_type = 0x02;
    csdata->rbuf->status   = 0;
    unsigned int *pl       = (unsigned int *)csdata->rbuf->payload;
    csdata->rbuf->payload_size = 12;
    
    *pl = ((struct man_msg*)evt->data)->p1; // code
    pl++;
    *pl = ((struct man_msg*)evt->data)->p2; // stack size
    pl++;
    *pl = (unsigned int)new_vchan->own_chanend; // this CS chanend

    _xtask_ring_send(csdata); // send the ring bus message

    // we will have to wait for the ring bus message to get back
    // we allocate a new pending ring bus request structure
    // to save the state
    struct p_request *pr = malloc(sizeof(struct p_request));
    pr->tid = ((struct man_msg*)evt->data)->p0;
    pr->msg_type = 0x02;
    pr->data = (void *)new_vchan;
    
    struct cs_kernel *temp_k = csdata->kernels;
    
    // find the kernel that has the pending reply
    while (temp_k != NULL) {
      if (temp_k->c_sync == evt->res) {
        break;
      }

      temp_k = temp_k->next;
    }

    new_vchan->kernel = temp_k; // also the vchan wants to know the kernel
    pr->kernel = temp_k;

    // add pending ring bus request to end of list
    prp = &csdata->p_reqs;

    while (*prp != NULL) {
      prp = &(*prp)->next;
    }

    pr->next = *prp;
    *prp = pr;
    
    return NO_REPLY; // the kernel does not expect a reply
    
  } else if (((struct man_msg*)evt->data)->cmd == 7) {
    /* 
       The task requests to get the outbox.
       p0 = mailbox id
    */
    struct mailbox *reg;
    reg = xtask_get_mailbox(csdata,((struct man_msg*)evt->data)->p0);
    if (reg != NULL) {
      ((struct man_msg*)evt->data)->p0 = (unsigned int) &reg->outbox;
    } else {
      ((struct man_msg*)evt->data)->p0 = 0;
    }
      
    return REPLY;
    
  } else if (((struct man_msg*)evt->data)->cmd == 8) {
    /* 
       The task requests to send his outbox
       to a recipient.
       p0 = sender mailbox id
       p1 = recipient mailbox id
    */
    unsigned int receiver;
    unsigned int sender;
    struct mailbox *recv_mb;
    struct mailbox *send_mb;
    receiver = ((struct man_msg*)evt->data)->p1;
    sender = ((struct man_msg*)evt->data)->p0;

    recv_mb = xtask_get_mailbox(csdata, receiver);    
    send_mb = xtask_get_mailbox(csdata, sender);    

    // check if receiver is on the same tile
    if (recv_mb != NULL) {
      // recipient is on the same tile, makes things easier :)
      if (recv_mb->inbox_state & INBOX_TASK_WAITING) {
        // recipient is blocked waiting for a message
        
        struct p_kreply *kr;
        
        // copy sender outbox to recipient inbox
        memcpy(recv_mb->inbox.data, send_mb->outbox.data, send_mb->outbox.data_size);
        recv_mb->inbox.data_size = send_mb->outbox.data_size;

        recv_mb->inbox_state &= ~(INBOX_TASK_WAITING); // not waiting anymore soon
                
        kr = xtask_get_free_kreply(csdata);

        // add a new pending kernel reply for the recipient task to unblock it
        // and notify the kernel
        if (kr != NULL) {
          kr->state |= KR_USED;
          kr->k = recv_mb->kernel;
          kr->reply.cmd = 0x03;
          kr->reply.p0 = recv_mb->tid;
          kr->reply.p1 = (unsigned int) &recv_mb->inbox;   
          
          _xtask_notify_kernel(recv_mb->kernel->c_async);
        }

        kr = xtask_get_free_kreply(csdata);

        // add a new pending kernel reply for the sending task
        // and notify the kernel
        if (kr != NULL) {
          kr->state |= KR_USED;
          kr->k = send_mb->kernel;
          kr->reply.cmd = 0x04;
          kr->reply.p0 = send_mb->tid;
          kr->reply.p1 = 0; /* return value.. delivery failed or not... */
          
          _xtask_notify_kernel(send_mb->kernel->c_async);
        }
        
          
      } else {
        struct mailbox **rpp;
        // recipient task is not ready to receive
        // add sender to pending outboxes

        // save recipient mailbox id
        send_mb->outbox_dest = recv_mb->id;

        // add sender to list of pending outboxes
        rpp = &csdata->p_outbox;
        
        while (*rpp != NULL) {
          rpp = &(*rpp)->p_next;
        }

        send_mb->p_next = *rpp;
        *rpp = send_mb;  

        // indicate at the recipient inbox that a sender is pending
        recv_mb->inbox_state |= INBOX_SENDER_PEND;
      }
    } else {
      // recipient is not on this tile, maybe on another tile
      // use the ring bus to inform other communication servers
      // if we don't have a ring bus, add a pending kernel reply with error
      
      if (!csdata->ring) {
        struct p_kreply *kr;
        kr = xtask_get_free_kreply(csdata);

        // add a new pending kernel reply for the sending task
        // and notify the kernel
        if (kr != NULL) {
          kr->state |= KR_USED;
          kr->k = send_mb->kernel;
          kr->reply.cmd = 0x04;
          kr->reply.p0 = send_mb->tid;
          kr->reply.p1 = 1; /* return value.. delivery failed or not... */
          
          _xtask_notify_kernel(send_mb->kernel->c_async);
        }
      } else {
        struct p_request **prp;
        struct p_request *pr;
        unsigned int *pl = (unsigned int *)csdata->rbuf->payload;
  
        send_mb->outbox_dest = receiver;
  
        csdata->rbuf->cs_id = csdata->id;
        csdata->rbuf->msg_type = 0x03;
        csdata->rbuf->status = 0;
        
        // first 4 bytes = mailbox id, remaining = message
        csdata->rbuf->payload_size = send_mb->outbox.data_size + 4;
            
        *pl = ((struct man_msg*)evt->data)->p1; // recipient mailbox id
        pl++;
  
        // copy outbox to ring bus payload buffer
        memcpy(pl, send_mb->outbox.data, send_mb->outbox.data_size);
        
        _xtask_ring_send(csdata);
        
        // add pending reply from ring bus to the list
        pr           = malloc(sizeof(struct p_request));
        pr->tid      = send_mb->tid;
        pr->msg_type = 0x03;
        pr->data     = (void *)send_mb;
        pr->kernel   = send_mb->kernel;
      
        prp = &csdata->p_reqs;
  
        while (*prp != NULL) {
          prp = &(*prp)->next;
        }
  
        pr->next = *prp;
        *prp = pr;
      }        
    }

    return NO_REPLY;
  } else if (((struct man_msg*)evt->data)->cmd == 9) {
    /*
       Task requests to read his inbox
       p0 = mailbox id
       p1 = location (local tile, anywhere)
    */
    struct mailbox *reg;
    struct mailbox **rpp;
    unsigned int *pl;

    reg = xtask_get_mailbox(csdata,((struct man_msg*)evt->data)->p0);
    
    if (reg != NULL) {
      reg->inbox_state |= INBOX_TASK_WAITING;      
    } else {
      // mailbox not found
    }

    if (reg->inbox_state & INBOX_SENDER_PEND) {
      // someone tried to send a message but
      // the task was not ready
      
      reg->inbox_state &= ~(INBOX_SENDER_PEND);

      rpp = &csdata->p_outbox;      

      /* check for local pending sender(s) */
      while (*rpp != NULL) {
        if ((*rpp)->outbox_dest == reg->id) {
          // found pending sender
          if (reg->inbox_state & INBOX_TASK_WAITING) {
            struct p_kreply *kr;    
          
            reg->inbox_state &= ~(INBOX_TASK_WAITING);
            
            // copy sender outbox to recipient inbox
            memcpy(reg->inbox.data, (*rpp)->outbox.data, (*rpp)->outbox.data_size);
            reg->inbox.data_size = (*rpp)->outbox.data_size;

            kr = xtask_get_free_kreply(csdata);

            if (kr != NULL) {
              // add pending kernel reply to unblock recipient task
              kr->state    |= KR_USED;
              kr->k        = reg->kernel;
              kr->reply.p0 = reg->tid;
              kr->reply.p1 = (unsigned int) &reg->inbox;
        
              // notify kernel for the pending reply
              _xtask_notify_kernel(reg->kernel->c_async);
            }

            kr = xtask_get_free_kreply(csdata);

            if (kr != NULL) {
              // add pending kernel reply to unblock sending task
              kr->state |= KR_USED;
              kr->k = (*rpp)->kernel;
              kr->reply.p0 = (*rpp)->tid;
              kr->reply.p1 = 1;  // return value.. delivery failed or not... 
              
              // notify kernel for the pending reply
              _xtask_notify_kernel((*rpp)->kernel->c_async);
            }

            // remove mailbox from pending outboxes list
            *rpp = (*rpp)->p_next;
            
            // don't set the pointer-pointer to the next element!
        
          } else {
            // found more than one pending senders
            reg->inbox_state |= INBOX_SENDER_PEND;
            rpp = &(*rpp)->p_next;
          }
        } else {
          rpp = &(*rpp)->p_next;
        }

      } /* end check for local pending senders */

      if (((struct man_msg*)evt->data)->p1 == ITC_ANYWHERE && csdata->ring) {
        /* notify CS on other tiles that this task was/is ready to receive */
        csdata->rbuf->cs_id        = csdata->id;
        csdata->rbuf->msg_type     = 0x04;
        csdata->rbuf->status       = 0x00;
        csdata->rbuf->payload_size = 0x04;
        pl = csdata->rbuf->payload;
        *pl = reg->id;
    
        _xtask_ring_send(csdata); // send ring bus message
        // don't need to add to pending ring bus reply list
        // because no action is taken when reply from ring bus
      }
    }

    return NO_REPLY;
  
  } else if (((struct man_msg*)evt->data)->cmd == 10) {
    /*
       Kernel has received a notification from CS
       and now wants to receive the pending reply.
    */   

    struct cs_kernel *temp_k = csdata->kernels;
    struct p_kreply *kr;    

    // find out which kernel by chanend
    while (temp_k != NULL) {
      if (temp_k->c_sync == evt->res) {
        break;
      }

      temp_k = temp_k->next;
    }

    if (temp_k != NULL) {

      kr = xtask_get_kreply(csdata, temp_k);

      // check if a pending kernel reply has been found 
      if (kr != NULL) {

        // copy kernel reply data
        ((struct man_msg*)evt->data)->cmd = kr->reply.cmd;
        ((struct man_msg*)evt->data)->p0 = kr->reply.p0;
        ((struct man_msg*)evt->data)->p1 = kr->reply.p1;
        ((struct man_msg*)evt->data)->p2 = kr->reply.p2;
        ((struct man_msg*)evt->data)->p3 = kr->reply.p3;
        ((struct man_msg*)evt->data)->p4 = kr->reply.p4;
        ((struct man_msg*)evt->data)->p5 = kr->reply.p5;
        kr->state &= ~(KR_USED);        
      }
    } 
    
    return REPLY;
  }

  return NO_REPLY; /* should not reach! */  
}

/******************************************************************************
 * Function:     xtask_cs_get_rd_ptr                                          *
 * Parameters:   vc  -   Pointer to vchan structure.                          *
 * Return:       Pointer where CS can write next object or null pointer       *
 *               when no buffer space is available                            *
 *                                                                            *
 *               The event vector for receiving data through a channel        *
 *               from a hardware thread uses this function to get a pointer   *
 *               to write the next object. This function must be called       *
 *               prior to the reception of every object.                      *
 ******************************************************************************/
void * xtask_cs_get_rd_ptr(struct vchan *vc)
{
  unsigned int buf;
  unsigned int rd_ptr;

  // check which buffer to use
  if (vc->state & CS_RD_BUF0) {
    // CS has already partially filled buffer 0, continue using this one
    buf = 0;
  } else if (vc->state & CS_RD_BUF1) {
    // CS has already partially filled buffer 1, continue using this one
    buf = 1;
  } else {
    // CS has no read buffer assigned
    // See if we can find one that isn't in use by a task or already filled 
    if ( !(vc->state & TASK_RD_BUF1) && !(vc->state & RD_BUF1_FILLED)) {
      vc->state |= CS_RD_BUF1;
      buf = 1;
    } else if ( !(vc->state & TASK_RD_BUF0) && !(vc->state & RD_BUF0_FILLED)) {
      vc->state |= CS_RD_BUF0;
      buf = 0;  
    } else {
      // none available, return zero
      vc->state |= CS_RD_BLOCK;
      return (void *)0;
    } 
  } 
  
  // check if there is room in the buffer and calc new pointer
  if ((vc->read_bufs[buf].buf_size - vc->read_bufs[buf].data_size) < vc->obj_size) {
    // buffer is unexpectedly full, should not get here
  } else {
    // new pointer is buffer pointer + current data size
    rd_ptr = (unsigned int) vc->read_bufs[buf].data;
    rd_ptr += vc->read_bufs[buf].data_size;
    vc->read_bufs[buf].data_size += vc->obj_size; // the new data size after receiving one object

    return (void *)rd_ptr;
  }

  return (void *)0;
}

/******************************************************************************
 * Function:     xtask_cs_check_rd_blocked_tasks                              *
 * Parameters:   vc      - Pointer to vchan structure.                        *
 *               csdata  - Pointer to cs_data structure                       *
 * Return:       void                                                         *
 *                                                                            *
 *               The event vector for receiving data through a channel        *
 *               from a hardware thread calls this function after receiving   *
 *               an object to see if there was a task blocked on reading      *
 *               data which can now be unblocked.                             *
 ******************************************************************************/
void xtask_cs_check_rd_blocked_tasks(struct vchan *vc, struct cs_data *csdata)
{
  unsigned int buf;
  
  // check which buffer the CS has written to  
  if (vc->state & CS_RD_BUF0) {
    buf = 0;
  } else if (vc->state & CS_RD_BUF1) {
    buf = 1;
  }

  if ((vc->read_bufs[buf].buf_size - vc->read_bufs[buf].data_size) < vc->obj_size) {
    // buffer is full
    
    if (buf == 0) {
      // indicate that buffer 0 is full and not in use by CS
      vc->state &= ~(CS_RD_BUF0);
      vc->state |= RD_BUF0_FILLED;
      
      // check if order flag has to be set
      if (vc->state & RD_BUF1_FILLED) {
        vc->state |= RD_BUF1_FIRST;
      }
    } else if (buf == 1) {
      // indicate that buffer 1 is full and not in use by CS
      vc->state &= ~(CS_RD_BUF1);
      vc->state |= RD_BUF1_FILLED;
      
      // check if order flag has to be set
      if (vc->state & RD_BUF0_FILLED) {
        vc->state |= RD_BUF0_FIRST;
      }
    }
  }
    
  if (vc->state & TASK_RD_BLOCK) {
    // a task is blocked on a read operation
    
    if ((vc->min_read_size > 0 && vc->read_bufs[buf].data_size >= vc->min_read_size) || 
         vc->read_bufs[buf].data_size == vc->read_bufs[buf].buf_size) {
      // we can unblock the task because either the buffer is completely filled
      // or there is enough data in the buffer to meet the minimum data amount for the read operaton

      struct man_msg msg;
      struct p_kreply *kr;
      msg.cmd = 1;
      msg.p0 = vc->handle; // return
      
      if (buf == 0) {
        vc->state &= ~(CS_RD_BUF0);     // clear flag that CS uses buffer 0
        vc->state &= ~(RD_BUF0_FILLED); // clear flag that buf 0 is filled
        vc->state &= ~(TASK_RD_BLOCK);  // clear flag that task is blocked on read operation
        vc->state |= TASK_RD_BUF0;      // set flag that task has buf 0
        msg.p1 = (unsigned int)&vc->read_bufs[0]; // return buffer 0 to kernel
      } else if (buf == 1) {
        vc->state &= ~(CS_RD_BUF1);     // clear flag that CS uses buffer 1
        vc->state &= ~(RD_BUF1_FILLED); // clear flag that buf 1 is filled
        vc->state &= ~(TASK_RD_BLOCK);  // clear flag that task is blocked on read operation
        vc->state |= TASK_RD_BUF1;      // set flag that task has buf 1
        msg.p1 = (unsigned int)&vc->read_bufs[1]; // return buffer 1 to kernel

      }
      
      kr = xtask_get_free_kreply(csdata);
          
      if (kr != NULL) {
        // add pending kernel reply and notify kernel
        kr->state |= KR_USED;
        kr->k = vc->kernel;
        kr->reply.cmd = msg.cmd;
        kr->reply.p0 = msg.p0;
        kr->reply.p1 = msg.p1;
        
        _xtask_notify_kernel(vc->kernel->c_async);
      }
    }
  }
}


/******************************************************************************
 * Function:     xtask_process_ring_msg                                       *
 * Parameters:   csdata  - Pointer to cs_data structure                       *
 * Return:       void                                                         *
 *                                                                            *
 *               Process a message received from the ring bus. This can be    *
 *               a message initiated by another CS or a message initiated     *
 *               by this CS that has been passed to all CS and is now         *
 *               returning.                                                   *
 *                                                                            * 
 *               This function should be divided in subfunctions to           *
 *               increase readability.                                        * 
 ******************************************************************************/
void xtask_process_ring_msg (struct cs_data *csdata)
{
  unsigned int *up;

  up = csdata->rbuf->payload;
  //printf("CH[%u] ring packet: cs_id: %u msg_type: %u status: %u  payload_size: %u\n", 
  //csdata->id, csdata->rbuf->cs_id, csdata->rbuf->msg_type, csdata->rbuf->status, csdata->rbuf->payload_size);
  //printf("CH[%u] ring rec: %u\n", csdata->id, *up);
  
  if (csdata->rbuf->cs_id == csdata->id) {
    // received own initiated message
    
    if (csdata->rbuf->msg_type == 0x01) {
      // used to test ring bus connectivity
      // expects payload with the CS id's of each CS in the ring bus
      int i;
      unsigned int *up = csdata->rbuf->payload;
      for (i=0; i < csdata->rbuf->payload_size; i+=4) {
        //printf("CH[%u] Found CH: %u\n",csdata->id, *up);
        up++;
      }
    } else if (csdata->rbuf->msg_type == 0x02) {
        // Ring bus reply from creating a new hardware thread
        struct p_request *pr;
        struct vchan *vc;
        unsigned int *pl = (unsigned int *)csdata->rbuf->payload;
        struct p_kreply *kr;
        
        // the pending ring bus reply is in front of the list because we always receive
        // replies in order. We gain access again to the previously allocated virtual
        // channel structure and complete the initialisation.
        vc = (struct vchan *) csdata->p_reqs->data;

        // we now have the destination chanend
        vc->thread_chanend = *pl;
        _xtask_set_chanend_dest(vc->own_chanend, vc->thread_chanend);

        vc->handle = vc->thread_chanend;
        
        // initialise chan_event structure for the event vector
        vc->event         =  (struct chan_event*) malloc(sizeof(struct chan_event));
        vc->event->res    = vc->own_chanend;
        vc->event->vector = (void *) _xtask_vc_vect;
        vc->event->env    = (void *) vc;
        
        // set up and enable events from chanend
        _xtask_set_chan_event((void*)vc->event);
        
        // add virtual channel to the list
        vc->next = csdata->vchans;
        csdata->vchans = vc;
        
        pr = csdata->p_reqs;
        
        // add kernel reply to queue and notify kernel
        kr = xtask_get_free_kreply(csdata);

        if (kr != NULL) {
          kr->state    |= KR_USED;
          kr->k         = vc->kernel;
          kr->reply.cmd = 2;
          kr->reply.p0  = vc->thread_chanend;
          kr->reply.p1  = pr->tid;
          kr->reply.p2 = 0; // return value, succeeded
          
          _xtask_notify_kernel(vc->kernel->c_async);
        }
        
        // remove pending ring bus reply from list and release memory
        csdata->p_reqs = csdata->p_reqs->next;
        free((void *)pr);
        
    } else if (csdata->rbuf->msg_type == 0x03) {
        // Task wants to send outbox to recipient on remote tile
        // We have now received a reply
      
        if (csdata->rbuf->status == 0x00) {
          // recipient not found!
          // remove pending ring bus reply from list and free memory
          struct p_request *pr; 
          
          pr = csdata->p_reqs;
          csdata->p_reqs = csdata->p_reqs->next;
          free(pr);
          
          // no reply to kernel, so task blocked forever?
          
        } else if (csdata->rbuf->status == 0x01) {
          // message delivered
          
          struct p_request *pr;
          struct mailbox *reg;
          struct p_kreply *kr;      
    
          // get mailbox from pending ring bus replies list
          pr = csdata->p_reqs;
          reg = pr->data;

          // remove pending reply from list
          csdata->p_reqs = csdata->p_reqs->next;
          free(pr);

          // add kernel reply to queue and notify kernel
          kr = xtask_get_free_kreply(csdata);

          if (kr != NULL) {
            kr->state    |= KR_USED;
            kr->k         = reg->kernel;
            kr->reply.cmd = 0x04;
            kr->reply.p0  = reg->tid;
            kr->reply.p1  = 0; // return value, delivery succeeded
          
            _xtask_notify_kernel(reg->kernel->c_async);
          }

        } else if (csdata->rbuf->status == 0x02) {
          // recipient was found but was not ready to receive message
          struct p_request *pr;
          struct mailbox *reg;
          struct mailbox **rpp;  
                  
          // get mailbox from pending ring bus reply list
          pr = csdata->p_reqs;
          reg = pr->data;

          // remove pending ring bus reply from list and free memory
          csdata->p_reqs = csdata->p_reqs->next;
          free(pr);

          // add mailbox to end of pending outbox list
          rpp = &csdata->p_outbox;
        
          while (*rpp != NULL) {
            rpp = &(*rpp)->p_next;
          }

          reg->p_next = *rpp;
          *rpp = reg; 
          
        }
    } else if (csdata->rbuf->msg_type == 0x04) {
      // notified other CS that task is ready to receive, do nothing
    }
  } else {
    // The received ring bus message originate from another CS
    
    if (csdata->rbuf->msg_type == 0x01) {
      // used for testing ring bus connectivity
      // add own CS id to payload and pass the message
      unsigned int *up = (unsigned int *)csdata->rbuf->payload;
      up += (csdata->rbuf->payload_size / 4);
      *up = csdata->id;
      csdata->rbuf->payload_size += 4;
    } else if (csdata->rbuf->msg_type == 0x02) {
      // create a new remote hardware thread
      // only when the status is still 0
      // otherwise some other CS has already
      // created the hardware thread and we just
      // pass the message without taking action
      if (csdata->rbuf->status == 0) {
        unsigned int code;
        unsigned int stacksize;
        unsigned int cs_c;
        chanend own_c;
        unsigned int *pl = (unsigned int *)csdata->rbuf->payload;
        code = *pl++;
        stacksize = *pl++;
        cs_c = *pl;

        unsigned int * new_stack = (unsigned int *)malloc(128 * WORD_SIZE);
        void * new_sp = (void*) (new_stack + (128 - 1));

        own_c = _xtask_get_chanend();
        _xtask_set_chanend_dest(own_c, cs_c);
        
        _xtask_create_thread((void*)test_hardware_thread, (void*)new_sp, (void*)0, own_c);

        // return hardware thread chanend
        csdata->rbuf->payload_size = 4;
        pl = (unsigned int *)csdata->rbuf->payload;
        *pl = own_c;
        csdata->rbuf->status = 1; // other CS should not take action anymore

      }
    } else if (csdata->rbuf->msg_type == 0x03 && csdata->rbuf->status == 0) {
      // A task wants to send his outbox to a task on
      // another tile.    
      // message status: 0: not found yet 1: delivered 2: task not ready
      unsigned int *pl;
      unsigned int receiver;
      struct mailbox *recv_mb;
      
      pl = csdata->rbuf->payload;
      receiver = *pl;  // get the receiver from the payload
      pl++;

      // is receiver on this tile?
      recv_mb = xtask_get_mailbox(csdata, receiver);

      if (recv_mb != NULL) {
        // found receiver on this tile
        
        if (recv_mb->inbox_state & INBOX_TASK_WAITING) {
          // task is waiting for a message
          
          struct p_kreply *kr;

          // copy message from ring bus to recipient inbox
          memcpy(recv_mb->inbox.data, pl, csdata->rbuf->payload_size-4);
          recv_mb->inbox.data_size = csdata->rbuf->payload_size-4;

          // task not waiting anymore after this
          recv_mb->inbox_state &= ~(INBOX_TASK_WAITING);
        
          kr = xtask_get_free_kreply(csdata);

          // add pending kernel reply and notify kernel
          if (kr != NULL) {
            kr->state    |= KR_USED;
            kr->k         = recv_mb->kernel;
            kr->reply.cmd = 0x03;
            kr->reply.p0  = recv_mb->tid;
            kr->reply.p1  = (unsigned int) &recv_mb->inbox;   
            
            _xtask_notify_kernel(recv_mb->kernel->c_async);
          }

          csdata->rbuf->status = 1;       // indicate that message has been delivered
          csdata->rbuf->payload_size = 0; // don't need to keep the message in the payload
        } else {
          // recipient is not ready to receive the message
          recv_mb->inbox_state     |= INBOX_SENDER_PEND; // recipient will know that someone tried to send
          csdata->rbuf->status       = 2; // indicate that the recipient is not ready
          csdata->rbuf->payload_size = 0; // don't need to keep the message in the payload
        }   
      } 
    } else if (csdata->rbuf->msg_type == 0x04) {
      /* a receiver task is read, check if there are pending senders */
      struct mailbox **rpp;
      unsigned int *pl;
      unsigned int rbuf_cs_id, rbuf_msg_type, rbuf_status, 
                   rbuf_payload_size, rbuf_recv_task, state;

      // copy the ring bus buffer, 
      // because we might create new bus messages before passing this one
      rbuf_cs_id        = csdata->rbuf->cs_id;
      rbuf_msg_type     = csdata->rbuf->msg_type;
      rbuf_status       = csdata->rbuf->status;
      rbuf_payload_size = csdata->rbuf->payload_size;
      pl                = csdata->rbuf->payload;
      rbuf_recv_task    = *pl;

      state = 0; // indicates whether we modified the original message, 0 means no      

      rpp = &csdata->p_outbox;

      // try to find pending senders
      while (*rpp != NULL) {
            
        if ((*rpp)->outbox_dest == rbuf_recv_task) {
          struct p_request *pr;

          // send pending sender task message through ring bus to recipient
          csdata->rbuf->cs_id        = csdata->id;
          csdata->rbuf->msg_type     = 0x03;
          csdata->rbuf->status       = 0x00;
          csdata->rbuf->payload_size = 0x04 + (*rpp)->outbox.data_size;
          pl  = csdata->rbuf->payload;
          *pl = (*rpp)->outbox_dest;
          pl++;
          memcpy(pl, (*rpp)->outbox.data, (*rpp)->outbox.data_size);
          
          _xtask_ring_send(csdata);
          
          // add pending bus reply to list
          pr = xtask_get_free_p_request(csdata);
          pr->kernel   = (*rpp)->kernel;
          pr->tid      = (*rpp)->tid;
          pr->msg_type = 0x03;
          pr->data     = *rpp;  

          // remove mailbox from pending outboxes list
          *rpp = (*rpp)->p_next;
          
          /* we have modified the ring bus buffer */
          state = 1;
      
          // don't set the pointer-pointer to the next element!
        } else {
          rpp = &(*rpp)->p_next;
        }
      }
      
      // restore ring bus buffer if modified
      if (state != 0) {
        csdata->rbuf->cs_id        = rbuf_cs_id;
        csdata->rbuf->msg_type     = rbuf_msg_type;
        csdata->rbuf->status       = rbuf_status;
        csdata->rbuf->payload_size = rbuf_payload_size;
        pl  = csdata->rbuf->payload;
        *pl = rbuf_recv_task;
      }
    } 
    // we always pass a message (modified or not) that did not originate from this CS
    _xtask_ring_send(csdata);
  } 
}

/******************************************************************************
 * Function:     xtask_get_mailbox                                            *
 * Parameters:   csdata  - Pointer to cs_data structure                       *
 *               id      - mailbox id                                         *
 * Return:       pointer to struct mailbox, or NULL when mailbox              *
 *               could not be found                                           *
 *                                                                            *
 *               Find the mailbox given the mailbox id.                       *
 ******************************************************************************/
struct mailbox * xtask_get_mailbox(struct cs_data * csdata, 
                                   unsigned int     id)
{
  struct mailbox *temp_mb = csdata->mailboxes;
  
  while (temp_mb != NULL) {
    if (temp_mb->id == id) {
      break;
    }

    temp_mb = temp_mb->next;
  }

  return temp_mb;
}

/******************************************************************************
 * Function:     xtask_get_free_kreply                                        *
 * Parameters:   csdata  - Pointer to cs_data structure                       *
 * Return:       pointer to free p_kreply structure, or NULL when             *
 *               could not be found                                           *
 *                                                                            *
 *               Find a free pending kernel reply structure                   *
 ******************************************************************************/
struct p_kreply * xtask_get_free_kreply(struct cs_data *csdata)
{
  int i;
  
  for (i = 0; i < 8; i++) {
    if (! (csdata->k_replies[i].state & KR_USED)) {
      return &csdata->k_replies[i];
    }
  }
  
  return NULL; // not found, big trouble
}

/******************************************************************************
 * Function:     xtask_get_kreply                                             *
 * Parameters:   csdata  - Pointer to cs_data structure                       *
 *               k       - Pointer to kernel structure                        *
 * Return:       pointer to the p_kreply structure with the pending reply     *
 *               for the specific kernel                                      *
 *                                                                            *
 *               Find pending kernel reply for the given kernel.              *
 ******************************************************************************/
struct p_kreply * xtask_get_kreply(struct cs_data * csdata, 
                                struct cs_kernel  * k)
{
  int i;
  
  for (i = 0; i < 8; i++) {
    if ((csdata->k_replies[i].state & KR_USED) && 
        (csdata->k_replies[i].k == k)) {
      return &csdata->k_replies[i];
    }
  }
  
  return NULL;
}

/******************************************************************************
 * Function:     xtask_get_free_p_request                                     *
 * Parameters:   csdata  - Pointer to cs_data structure                       *
 * Return:       Pointer to a new allocated p_request structure               *
 *                                                                            *
 *               Get a new pending ring bus reply structure by allocating     *
 *               one and adding it to the list of pending replies.            *
 ******************************************************************************/
struct p_request * xtask_get_free_p_request(struct cs_data *csdata)
{
  struct p_request *pr;
  struct p_request **ppr;

  pr = malloc(sizeof(struct p_request));

  // add new structure to end of list of pending replies
  ppr = &csdata->p_reqs;

  while (*ppr != NULL) {
    ppr = &(*ppr)->next;
  }

  pr->next = *ppr;
  *ppr = pr;

  return pr;
}

void test_hardware_thread(void *args, chanend c)
{
  //unsigned int temp;
  unsigned int i = 0;
  //printf("Test process running!\n");
  while(i<10000) {
    //temp = receive_something(c);
    //send_something(c, i);
    printf("Remote thread running\n");
    i++;
  }
}
