/******************************************************************************
 *                                                                            *
 * File:   comserver_asm.S                                                    *
 * Author: Bianco Zandbergen <bianco [AT] zandbergen.name>                    *
 *                                                                            *
 * This file is part of the xTask Distributed Operating System for            *
 * the XMOS XS1 microprocessor architecture (www.xtask.org).                  *
 *                                                                            *
 * This file contains the assembly part of the implementation of the          *
 * Communication Server. More specific it contains the following functions:   *
 *                                                                            *
 * _xtask_man_sendrec        - send and receive man. msg (used by kernel)     *
 * _xtask_man_send           - send management message                        *
 * _xtask_man_chan_vec       - event vector for receiving management messages *
 * _xtask_set_chan_event     - initialise chanend for using events            *
 * _xtask_get_chanend        - allocate chanend                               *
 * _xtask_set_chanend_dest   - set chanend destination                        *
 * _xtask_vc_vect            - event vector for receiving from hardware thread*
 * _xtask_set_cs_data        - push address of cs_data struct on stack        *
 * _xtask_create_thread      - create new hardware thread                     *
 * _xtask_chan_enable_events - enable events on a chanend                     *
 * _xtask_send_man_msg       - send management message                        *
 * _xtask_notify_kernel      - send notification to kernel through channel    *
 * _xtask_ring_vec           - event vector to receive from ring bus          *
 * _xtask_ring_send          - put message on ring bus                        *                                                                                                
 *                                                                            *
 ******************************************************************************/  
#include <xs1.h>

.text

/******************************************************************************
 * Function:     _xtask_man_sendrec                                           *
 * Parameters:   r0 - chanend resource id                                     *
 *               r1 - address of struct man_msg                               *
 * Return:       void                                                         *
 *                                                                            *
 *               Sends a request in the form of struct man_msg to the CS      *
 *               through a channel using the synchronized method from the     *
 *               XMOS ABI.                                                    *
 *               Directly receives back the reply from CS and stores the      *
 *               reply in the same data structure, overwriting the initial    *
 *               request. This function blocks until CS is sending back a     *
 *               reply.                                                       *
 ******************************************************************************/
.extern  _xtask_man_sendrec
.globl   _xtask_man_sendrec.nstackwords
.globl   _xtask_man_sendrec.maxthreads
.globl   _xtask_man_sendrec.maxtimers
.globl   _xtask_man_sendrec.maxchanends
.linkset _xtask_man_sendrec.nstackwords, 0
.linkset _xtask_man_sendrec.maxthreads,  0
.linkset _xtask_man_sendrec.maxtimers,   0
.linkset _xtask_man_sendrec.maxchanends, 0
.globl   _xtask_man_sendrec,"f{0}(ui,p(0))"
.cc_top  _xtask_man_sendrec.func, _xtask_man_sendrec

_xtask_man_sendrec:

//------------ SEND -----------

    outct     res[r0],     0x1       // send control token 1
    chkct     res[r0],     0x1       // receive control token 1

    ldw       r2,          r1[6]     // send structure word by word
    out       res[r0],     r2        // starting with the highest word in memory

    ldw       r2,          r1[5]
    out       res[r0],     r2

    ldw       r2,          r1[4]
    out       res[r0],     r2

    ldw       r2,          r1[3]
    out       res[r0],     r2
    
    ldw       r2,          r1[2]
    out       res[r0],     r2

    ldw       r2,          r1[1]
    out       res[r0],     r2

    ldw       r2,          r1[0]
    out       res[r0],     r2

    outct     res[r0],     0x1       // send control token 1
    chkct     res[r0],     0x1       // receive control token 1

//------------ RECEIVE ---------

    chkct     res[r0],     0x1       // receive control token 1
    outct     res[r0],     0x1       // send control token 1

    in        r2,          res[r0]   // receive structure word by word
    stw       r2,          r1[6]     // starting with the highest word in memory
                                     // write the received data to the original structure
    in        r2,          res[r0]
    stw       r2,          r1[5]    
    
    in        r2,          res[r0]
    stw       r2,          r1[4]    
    
    in        r2,          res[r0]
    stw       r2,          r1[3]    

    in        r2,          res[r0]
    stw       r2,          r1[2]    

    in        r2,          res[r0]
    stw       r2,          r1[1]    

    in        r2,          res[r0]
    stw       r2,          r1[0]    

    chkct     res[r0],     0x1       // receive control token 1
    outct     res[r0],     0x1       // send control token 1

    retsp     0

.cc_bottom _xtask_man_sendrec.func

/******************************************************************************
 * Function:     _xtask_man_send                                              *
 * Parameters:   r0 - chanend resource id                                     *
 *               r1 - address of struct man_msg                               *
 * Return:       void                                                         *
 *                                                                            *
 *               Sends a request in the form of struct man_msg to the CS      *
 *               through a channel using the synchronized method from the     *
 *               XMOS ABI. Unlike _xtask_man_sendrec this function does not   *
 *               try to receive a reply from CS. A reply from CS is either    * 
 *               not necessary or may take some time and the kernel will      *
 *               wait for a notification from CS instead.                     *     
 ******************************************************************************/
.extern  _xtask_man_send
.globl   _xtask_man_send.nstackwords
.globl   _xtask_man_send.maxthreads
.globl   _xtask_man_send.maxtimers
.globl   _xtask_man_send.maxchanends
.linkset _xtask_man_send.nstackwords, 0
.linkset _xtask_man_send.maxthreads,  0
.linkset _xtask_man_send.maxtimers,   0
.linkset _xtask_man_send.maxchanends, 0
.globl   _xtask_man_send,"f{0}(ui,p(0))"
.cc_top  _xtask_man_send.func, _xtask_man_send

_xtask_man_send:

//------------ SEND -----------

    outct     res[r0],     0x1       // send control token 1
    chkct     res[r0],     0x1       // receive control token 1

    ldw       r2,          r1[6]     // send structure word by word
    out       res[r0],     r2        // starting with the highest word in memory

    ldw       r2,          r1[5]
    out       res[r0],     r2

    ldw       r2,          r1[4]
    out       res[r0],     r2

    ldw       r2,          r1[3]
    out       res[r0],     r2
    
    ldw       r2,          r1[2]
    out       res[r0],     r2

    ldw       r2,          r1[1]
    out       res[r0],     r2

    ldw       r2,          r1[0]
    out       res[r0],     r2

    outct     res[r0],     0x1       // send control token 1
    chkct     res[r0],     0x1       // receive control token 1

    retsp     0

.cc_bottom _xtask_man_send.func

/******************************************************************************
 * Function:     _xtask_man_chan_vec                                          *
 * Parameters:   ed - address to struct chan_event,                           *
 *                    loaded from chanend environment vector on interrupt.    *
 * Return:       does not return, waits for next event                        *
 *                                                                            *
 *               This function is executed when an event is received that     *
 *               data is available from a chanend. This function will         *
 *               receive a man_msg structure from the kernel, call a          *
 *               function to process it, and optionally send a                *
 *               man_msg structure back to the kernel containing the reply    *
 ******************************************************************************/
.extern  _xtask_man_chan_vec
.globl   _xtask_man_chan_vec.nstackwords
.globl   _xtask_man_chan_vec.maxthreads
.globl   _xtask_man_chan_vec.maxtimers
.globl   _xtask_man_chan_vec.maxchanends
.linkset _xtask_man_chan_vec.nstackwords, 128
.linkset _xtask_man_chan_vec.maxthreads,  0
.linkset _xtask_man_chan_vec.maxtimers,   0
.linkset _xtask_man_chan_vec.maxchanends, 0
.globl   _xtask_man_chan_vec,"f{0}()"
.cc_top  _xtask_man_chan_vec.func, _xtask_man_chan_vec

_xtask_man_chan_vec:
  
    extsp     1                   // extend stack with 1 word
    get       r11,       ed       // copy address of chan_event to r11
    ldw       r0,        r11[0]   // load chan_event->res (chanend) to r0

    chkct     res[r0],   0x1      // receive control token 1
    outct     res[r0],   0x1      // send control token 1
    in        r1,        res[r0]  // receive struct chan_event word by word
    in        r2,        res[r0]  // starting with the highest word in memory
    in        r3,        res[r0]  // this code can be optimized to avoid fnops
    in        r4,        res[r0]
    in        r5,        res[r0]
    in        r6,        res[r0]
    in        r7,        res[r0]
    ldw       r0,        r11[4]   // load chan_event->data in r0 (address of a struct man_msg)
    stw       r1,        r0[6]    // store received words to struct man_msg
    stw       r2,        r0[5]
    stw       r3,        r0[4]
    stw       r4,        r0[3]
    stw       r5,        r0[2]
    stw       r6,        r0[1]
    stw       r7,        r0[0]

    ldw       r0,        r11[0]   // load chan_event->res (chanend) in r0
    chkct     res[r0],   0x1      // receive control token 1
    outct     res[r0],   0x1      // send control token 1

    // process message

    add       r6,        r0,   0  // store chanend in r6 (auto restore, after function call)
    add       r7,        r11,  0  // store address of chan_event in r7 (auto restore, after function call)

    add       r1,        r11,  0  // copy address of chan_event to r1
    ldw       r0,        sp[1]    // get address of cs_data structure from stack and store in r0
    
    bl        xtask_process_man_msg // process received message

    bf        r0,        _xtask_man_chan_vec_exit  // if return value is 0,
                                                   // jump over the send message back code 

    add       r0,        r6,   0  // restore chanend back to r0
    add       r11,       r7,   0  // restore address of chan_event back to r11
    
    // send message back

    outct     res[r0],   0x1      // send control token 1
    chkct     res[r0],   0x1      // receive control token 1

    ldw       r1,        r11[4]   // load chan_event->data in r1 (address of a struct man_msg)

    ldw       r2,        r1[6]    // send the modified man_msg back to the kernel
    out       res[r0],   r2       // word for word, starting with the highest word in memory

    ldw       r2,        r1[5]
    out       res[r0],   r2

    ldw       r2,        r1[4]
    out       res[r0],   r2

    ldw       r2,        r1[3]
    out       res[r0],   r2

    ldw       r2,        r1[2]
    out       res[r0],   r2
    
    ldw       r2,        r1[1]
    out       res[r0],   r2
    
    ldw       r2,        r1[0]
    out       res[r0],   r2
  
    outct     res[r0],   0x1      // send control token 1
    chkct     res[r0],   0x1      // receive control token 1

_xtask_man_chan_vec_exit:

    ldaw      r0,        sp[0]    // load value of SP in r0
    add       r0,        r0,   4  // increase with 1 word
    set       sp,        r0       // set new SP (we can't use retsp here because we don't want to return)
  
    waiteu                        // wait for next event

.cc_bottom _xtask_man_chan_vec.func

/******************************************************************************
 * Function:     _xtask_set_chan_event                                        *
 * Parameters:   r0 - address of chan_event structure                         *
 * Return:       none                                                         *
 *                                                                            *
 *               Initializes a chanend for receiving requests from the        *
 *               kernel. Will wait for first event to fire.                   *
 ******************************************************************************/
.extern  _xtask_set_chan_event
.globl   _xtask_set_chan_event.nstackwords
.globl   _xtask_set_chan_event.maxthreads
.globl   _xtask_set_chan_event.maxtimers
.globl   _xtask_set_chan_event.maxchanends
.linkset _xtask_set_chan_event.nstackwords, 0
.linkset _xtask_set_chan_event.maxthreads,  0
.linkset _xtask_set_chan_event.maxtimers,   0
.linkset _xtask_set_chan_event.maxchanends, 0
.globl   _xtask_set_chan_event,"f{0}(p(0))"
.globl   _xtask_set_chan_event

.cc_top _xtask_set_chan_event.func, _xtask_set_chan_event

_xtask_set_chan_event:
    ldw       r1,        r0[0]    // load chan_event->res (chanend) to r0
    ldw       r11,       r0[2]    // load chan_event->vector (event vector address) in r11
    setv      res[r1],   r11      // set event vector for chanend
    ldw       r11,       r0[3]    // load chan_event->env (environment vector, address of chan_event structure) in r11
    setev     res[r1],   r11      // set environment vetor for chanend
    eeu       res[r1]             // enable events from chanend
    retsp     0

.cc_bottom _xtask_set_chan_event.func

/******************************************************************************
 * Function:     _xtask_get_chanend                                           *
 * Parameters:   none                                                         *
 * Return:       Returns the resouce id of the allocated chanend.             *
 *                                                                            *
 *               Tries to allocate a chanend and returns the resouce id of    *
 *               the chanend. Should be replaced by inline assembly.          *
 ******************************************************************************/
.extern  _xtask_get_chanend
.globl   _xtask_get_chanend.nstackwords
.globl   _xtask_get_chanend.maxthreads
.globl   _xtask_get_chanend.maxtimers
.globl   _xtask_get_chanend.maxchanends
.linkset _xtask_get_chanend.nstackwords, 0
.linkset _xtask_get_chanend.maxthreads,  0
.linkset _xtask_get_chanend.maxtimers,   0
.linkset _xtask_get_chanend.maxchanends, 0
.globl   _xtask_get_chanend,"f{ui}()"
.cc_top  _xtask_get_chanend.func, _xtask_get_chanend

_xtask_get_chanend:
    getr      r0,        0x2  // get a chanend resource, store it as return value in r0
    retsp     0

.cc_bottom _xtask_get_chanend.func

/******************************************************************************
 * Function:     _xtask_set_chanend_dest                                      *
 * Parameters:   r0 - chanend resource id                                     *
 *               r1 - destination chanend                                     *
 * Return:       none                                                         *
 *                                                                            *
 *               Sets the destination of a chanend.                           *
 *               Should be replaced by inline assembly.                       *
 ******************************************************************************/
.extern  _xtask_set_chanend_dest
.globl   _xtask_set_chanend_dest.nstackwords
.globl   _xtask_set_chanend_dest.maxthreads
.globl   _xtask_set_chanend_dest.maxtimers
.globl   _xtask_set_chanend_dest.maxchanends
.linkset _xtask_set_chanend_dest.nstackwords, 0
.linkset _xtask_set_chanend_dest.maxthreads,  0
.linkset _xtask_set_chanend_dest.maxtimers,   0
.linkset _xtask_set_chanend_dest.maxchanends, 0
.globl   _xtask_set_chanend_dest,"f{0}(ui,ui)"
.cc_top  _xtask_set_chanend_dest.func, _xtask_set_chanend_dest

_xtask_set_chanend_dest:
    setd      res[r0],   r1       // set destination of chanend
    retsp     0

.cc_bottom _xtask_set_chanend_dest.func

/******************************************************************************
 * Function:     _xtask_vc_vect                                               *
 * Parameters:   ed - address to struct chan_event,                           *
 *                    loaded from chanend environment vector on interrupt.    *
 * Return:       does not return, waits for next event                        *
 *                                                                            *
 *               Event vector to receive and store data from a dedicated      *
 *               hardware thread on behalve of a task.                        *
 ******************************************************************************/
.extern  _xtask_vc_vect
.globl   _xtask_vc_vect.nstackwords
.globl   _xtask_vc_vect.maxthreads
.globl   _xtask_vc_vect.maxtimers
.globl   _xtask_vc_vect.maxchanends
.linkset _xtask_vc_vect.nstackwords, 1
.linkset _xtask_vc_vect.maxthreads,  0
.linkset _xtask_vc_vect.maxtimers,   0
.linkset _xtask_vc_vect.maxchanends, 0
.globl   _xtask_vc_vect,"f{0}()"

.cc_top _xtask_vc_vect.func, _xtask_vc_vect

_xtask_vc_vect:
    extsp     1                   // expand stack with 1 word

    get       r11,       ed       // load address of struct vchan in r11
    add       r0,        r11,  0  // copy r11 to r0
    bl        xtask_cs_get_rd_ptr // try to get a pointer to a read buffer

    bt        r0,        _xtask_vc_vect2 // read buffer available, jump over the disable code below

                                  // no read buffer was available, disable events on this chanend
    get       r11,       ed       // load address of struct vchan in r11
    ldw       r0,        r11[0]   // load value of vchan->event pointer in r0
    ldw       r0,        r0[0]    // load value of vchan->event->res, the chanend, in r0
    edu       res[r0]             // disable events and interrupts from chanend
  
    ldaw      r0,        sp[0]    // load value of SP in r0
    add       r0,        r0,   4  // add 1 word to it
    set       sp,        r0       // save new SP (we can't use retsp here because we aren't returning)
    
    waiteu                        // wait for events on other resources

_xtask_vc_vect2:
    add       r1,        r0,   0  // copy buffer pointer to r1

    get       r11,       ed       // load address of struct vchan in r11
    ldw       r0,        r11[0]   // load value of vchan->event pointer in r0
    ldw       r0,        r0[0]    // load value of vchan->event->res, the chanend, in r0
    ldw       r2,        r11[1]   // load value of vchan->obj_size in r2

    chkct     res[r0],   0x1      // receive control token 1 
    outct     res[r0],   0x1      // send control token 1

    // args: r0 = chanend, r1 = buffer pointer, r2 = data_size
    bl __crt_chan_rcv             // receive data from chanend and store in buffer

    chkct     res[r0],   0x1      // receive control token 1
    outct     res[r0],   0x1      // send control token 1

    get       r11,       ed       // load address of struct vchan in r11
    add       r0,        r11,  0  // copy r11 to r0
    ldw       r1,        r0[2]    // load value of vchan->csdata in r1
    bl        xtask_cs_check_rd_blocked_tasks  // check if a task can be unblocked

    // decrease stack
    ldaw      r0,        sp[0]    // load value of SP in r0
    add       r0,        r0,   4  // add 1 word to it
    set       sp,        r0       // save new SP (we can't use retsp here because we aren't returning)
    
    waiteu                        // wait for an event on one of the resources
.cc_bottom _xtask_vc_vect.func

/******************************************************************************
 * Function:     _xtask_set_cs_data                                           *
 * Parameters:   r0 - address of a struct cs_data                             *
 * Return:       none                                                         *
 *                                                                            *
 *               Store address of struct cs_data on stack for easy            *
 *               access.                                                      *
 ******************************************************************************/
.extern  _xtask_set_cs_data
.globl   _xtask_set_cs_data.nstackwords
.globl   _xtask_set_cs_data.maxthreads
.globl   _xtask_set_cs_data.maxtimers
.globl   _xtask_set_cs_data.maxchanends
.linkset _xtask_set_cs_data.nstackwords, 1
.linkset _xtask_set_cs_data.maxthreads,  0
.linkset _xtask_set_cs_data.maxtimers,   0
.linkset _xtask_set_cs_data.maxchanends, 0
.globl   _xtask_set_cs_data,"f{0}(p(0))"
.cc_top  _xtask_set_cs_data.func, _xtask_set_cs_data

_xtask_set_cs_data:
    extsp     1                   // expand stack with 1 word
    stw       r0,        sp[0]    // save address on stack
    retsp     0                   // return but do not decrease stack!
.cc_bottom _xtask_set_cs_data.func

/******************************************************************************
 * Function:     _xtask_create_thread                                         *
 * Parameters:   r0 - program counter of new thread                           *
 *               r1 - stack pointer of new thread                             *
 *               r2 - argument of new thread                                  *
 *               r3 - chanend of new thread                                   *
 * Return:       resource id of new thread                                    *
 *                                                                            *
 *               Create a new hardware thread.                                *
 ******************************************************************************/
.extern  _xtask_create_thread
.globl   _xtask_create_thread.nstackwords
.globl   _xtask_create_thread.maxthreads
.globl   _xtask_create_thread.maxtimers
.globl   _xtask_create_thread.maxchanends
.linkset _xtask_create_thread.nstackwords, 1
.linkset _xtask_create_thread.maxthreads,  0
.linkset _xtask_create_thread.maxtimers,   0
.linkset _xtask_create_thread.maxchanends, 0
// unsigned int thread_handle _xtask_create_thread(pc, sp, args, chanend)
.globl   _xtask_create_thread,"f{ui}(p(0),p(0),p(0),ui)"

.cc_top _xtask_create_thread.func, _xtask_create_thread

_xtask_create_thread:
    entsp     1                   // expand stack with 1 word
    stw       r4,        sp[0]    // save r4 on stack for later restore
    getr      r4,        3        // get a thread synchroniser, store in r4
    getst     r11,       res[r4]  // get synchronised thread, bound on the thread synchroniser in r4
    init      t[r11]:sp, r1       // set stack pointer of new thread
    init      t[r11]:pc, r0       // set PC of new thread
    set       t[r11]:r0, r2       // set r0, argument of new thread
    set       t[r11]:r1, r3       // set r1, chanend of new thread
    ldaw      r0,        dp[0]    // load value of data pointer in r0
    init      t[r11]:dp, r0       // set data pointer of new thread
    add       r1,        r11,  0  // copy thread resource id to r1
    ldaw      r11,       cp[0]    // load value of constant pointer in r11 
    init      t[r1]:cp,  r11      // set constant pointer of new thread
    msync     res[r4]             // start new thread
  
    ldw       r4,        sp[0]    // restore r4

    add       r0,        r1,   0  // we return thread resource id
    retsp     1                   // return and decrease stack with 1 word

.cc_bottom _xtask_create_thread.func

/******************************************************************************
 * Function:     _xtask_chan_enable_events                                    *
 * Parameters:   r0 - chanend resource id                                     *
 * Return:       none                                                         *
 *                                                                            *
 *               Enable events on a chanend                                   *
 ******************************************************************************/
.extern  _xtask_chan_enable_events
.globl   _xtask_chan_enable_events.nstackwords
.globl   _xtask_chan_enable_events.maxthreads
.globl   _xtask_chan_enable_events.maxtimers
.globl   _xtask_chan_enable_events.maxchanends
.linkset _xtask_chan_enable_events.nstackwords, 0
.linkset _xtask_chan_enable_events.maxthreads,  0
.linkset _xtask_chan_enable_events.maxtimers,   0
.linkset _xtask_chan_enable_events.maxchanends, 0
.globl   _xtask_chan_enable_events,"f{0}(ui)"

.cc_top _xtask_chan_enable_events.func, _xtask_chan_enable_events

_xtask_chan_enable_events:
    eeu       res[r0]   // enable event on chanend in r0
    retsp     0

.cc_bottom _xtask_chan_enable_events.func

/******************************************************************************
 * Function:     _xtask_send_man_msg                                          *
 * Parameters:   r0 - chanend resource id                                     *
 *               r1 - address of struct man_msg                               *
 * Return:       void                                                         *
 *                                                                            *
 *               Sends a struct man_msg over a channel.                       *
 *               This function is problably redundant.                        *
 ******************************************************************************/
.extern  _xtask_send_man_msg
.globl   _xtask_send_man_msg.nstackwords
.globl   _xtask_send_man_msg.maxthreads
.globl   _xtask_send_man_msg.maxtimers
.globl   _xtask_send_man_msg.maxchanends
.linkset _xtask_send_man_msg.nstackwords, 0
.linkset _xtask_send_man_msg.maxthreads,  0
.linkset _xtask_send_man_msg.maxtimers,   0
.linkset _xtask_send_man_msg.maxchanends, 0
.globl   _xtask_send_man_msg,"f{0}(ui,p(0))"
.cc_top  _xtask_send_man_msg.func, _xtask_send_man_msg

_xtask_send_man_msg:

//------------ SEND -----------

    // check destination for debug!
    //getd      r2, res[r0]

    outct     res[r0],     0x1
    chkct     res[r0],     0x1

    ldw       r2,          r1[6]
    out       res[r0],     r2

    ldw       r2,          r1[5]
    out       res[r0],     r2

    ldw       r2,          r1[4]
    out       res[r0],     r2

    ldw       r2,          r1[3]
    out       res[r0],     r2
    
    ldw       r2,          r1[2]
    out       res[r0],     r2

    ldw       r2,          r1[1]
    out       res[r0],     r2

    ldw       r2,          r1[0]
    out       res[r0],     r2

    outct     res[r0],     0x1
    chkct     res[r0],     0x1

    retsp     0

.cc_bottom _xtask_send_man_msg.func

/******************************************************************************
 * Function:     _xtask_notify_kernel                                         *
 * Parameters:   r0 - chanend resource id                                     *
 * Return:       none                                                         *
 *                                                                            *
 *               Send a control token to the kernel to notify it that         *
 *               some event has happened                                      *
 ******************************************************************************/
.globl   _xtask_notify_kernel
.globl   _xtask_notify_kernel.nstackwords
.globl   _xtask_notify_kernel.maxthreads
.globl   _xtask_notify_kernel.maxtimers
.globl   _xtask_notify_kernel.maxchanends
.linkset _xtask_notify_kernel.nstackwords, 0
.linkset _xtask_notify_kernel.maxthreads,  0
.linkset _xtask_notify_kernel.maxtimers,   0
.linkset _xtask_notify_kernel.maxchanends, 0
.globl   _xtask_notify_kernel,"f{0}(ui)"

.cc_top _xtask_notify_kernel.func, _xtask_notify_kernel

_xtask_notify_kernel:
    
    outct     res[r0],   0x1      // send control token 1
    retsp     0

.cc_bottom _xtask_notify_kernel.func

/******************************************************************************
 * Function:     _xtask_ring_vec                                              *
 * Parameters:   ed - address to struct cs_data,                              *
 *                    loaded from chanend environment vector on interrupt.    *
 * Return:       does not return, waits for next event                        *
 *                                                                            *
 *               Event vector to receive messages from the ring bus and       *
 *               calls a function to process the received message.            *
 ******************************************************************************/
.globl   _xtask_ring_vec
.globl   _xtask_ring_vec.nstackwords
.globl   _xtask_ring_vec.maxthreads
.globl   _xtask_ring_vec.maxtimers
.globl   _xtask_ring_vec.maxchanends
.linkset _xtask_ring_vec.nstackwords, 1
.linkset _xtask_ring_vec.maxthreads,  0
.linkset _xtask_ring_vec.maxtimers,   0
.linkset _xtask_ring_vec.maxchanends, 0
.globl   _xtask_ring_vec,"f{0}()"

.cc_top _xtask_ring_vec.func, _xtask_ring_vec

_xtask_ring_vec:

    extsp     1                   // increase stack with 1 word
  
    get       r11,       ed       // load address of cs_data in r11
    ldw       r4,        r11[2]   // copy value of cs_data->ring_in to r4
  
    chkct     res[r4],   0x1      // receive control token 1 
    outct     res[r4],   0x1      // send control token 1
    
    in        r5,        res[r4]  // receive communication server id
    in        r6,        res[r4]  // receive msg type
    in        r7,        res[r4]  // receive status
    in        r8,        res[r4]  // receive payload size
  
                                  // store received data in ring_buf struct
    ldw       r9,        r11[4]   // load value of cs_data->rbuf pointer in r9
    stw       r5,        r9[0]    // store rbuf->cs_id
    stw       r6,        r9[1]    // store rbuf->msg_type
    stw       r7,        r9[2]    // store rbuf->status
    stw       r8,        r9[3]    // store rbuf->payload_size 
    ldw       r9,        r9[4]    // load value of rbuf->payload pointer in r9

    // prepare parameters for recv call
    add       r0,        r4,   0  // resource
    add       r1,        r9,   0  // pointer to buffer
    add       r2,        r8,   0  // payload size

    bl        __crt_chan_rcv      // receive payload

    chkct     res[r4],   0x1      // receive control token 1
    outct     res[r4],   0x1      // send control token 1

    get       r11,       ed       // load address of cs_data in r11
    add       r0,        r11,  0  // copy to r0
    bl        xtask_process_ring_msg  // process received ring bus message
    
    ldaw      r0,        sp[0]    // load value of SP in r0
    add       r0,        r0,   4  // add 1 word to it
    set       sp,        r0       // save new SP (we can't use retsp here because we aren't returning)
    
    waiteu                        // wait for next event from any resource
.cc_bottom _xtask_ring_vec.func

/******************************************************************************
 * Function:     _xtask_ring_send                                             *
 * Parameters:   r0 - address to cs_data structure                            *
 * Return:       none                                                         *
 *                                                                            *
 *               Function to put a message on the ring bus.                   *
 ******************************************************************************/
.globl   _xtask_ring_send
.globl   _xtask_ring_send.nstackwords
.globl   _xtask_ring_send.maxthreads
.globl   _xtask_ring_send.maxtimers
.globl   _xtask_ring_send.maxchanends
.linkset _xtask_ring_send.nstackwords, 3
.linkset _xtask_ring_send.maxthreads,  0
.linkset _xtask_ring_send.maxtimers,   0
.linkset _xtask_ring_send.maxchanends, 0
.globl   _xtask_ring_send,"f{0}(p(0))"
.cc_top  _xtask_ring_send.func, _xtask_ring_send

_xtask_ring_send:

    entsp     3                   // expand stack with 3 words

    stw       r0,        sp[1]    // store address of cs_data structure on stack
    stw       r4,        sp[2]    // store r4 on stack for later restore
    ldw       r0,        r0[3]    // load cs_data->ring_out in r0

    ldw       r1,        sp[1]    // load adress of cs_data from stack
    ldw       r1,        r1[4]    // load value of cs_data->rbuf pointer in r1
    
    ldw       r2,        r1[3]    // load rbuf->payload_size in r2
    ldw       r3,        r1[1]    // load rbuf->msg_type in r3
    ldw       r11,       r1[4]    // load value of rbuf->payload pointer in r11
    ldw       r4,        r1[2]    // load rbuf->status in r4
    ldw       r1,        r1[0]    // load rbuf->cs_id in r1

    outct     res[r0],   0x1      // send control token 1
    chkct     res[r0],   0x1      // receive control token 1
  

    out       res[r0],   r1       // send communication server id
    out       res[r0],   r3       // send msg_type    
    out       res[r0],   r4       // send status
    out       res[r0],   r2       // send payload_size


    add       r1,        r11,  0  // copy payload buffer pointer to r1
    // args: r0: chanend r1: buffer address r2: size
    bl        __crt_chan_snd      // send payload

    outct     res[r0],   0x1      // send control token 1
    chkct     res[r0],   0x1      // receive control token 1
    
    ldw       r4,        sp[2]    // restore r4 from stack

    retsp     3                   // return and decrease stack with 3 words

.cc_bottom _xtask_ring_send.func

