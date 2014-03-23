/******************************************************************************
 *                                                                            *
 * File:   kernel_asm.S                                                       *
 * Author: Bianco Zandbergen <bianco [AT] zandbergen.name>                    *
 *                                                                            *
 * This file is part of the xTask Distributed Operating System for            *
 * the XMOS XS1 microprocessor architecture (www.xtask.org).                  *
 *                                                                            *
 * This file contains the assembly part of the implementation of the kernel.  *
 * More specific it contains the following functions:                         *
 *                                                                            *
 * SAVE_CONTEXT              - Macro to save the context of a task.           *
 *                             Used by various assembly functions.            *
 * RESTORE_CONTEXT           - Macro to restore the context of a task.        *
 *                             Used by various assembly functions.            *
 * _xtask_restore_context    - Function to start the scheduler by restoring   *
 *                             the context of the first task to run.          *
 * _xtask_timer_int          - timer interrupt handler.                       *
 * _xtask_init_system        - Initialise timer.                              *
 * _xtask_init_kdata         - Set up the kernel stack and kernel entry point.*
 * _xtask_init_task_stack    - Initialise the stack of a new task.            *
 * _xtask_get_kdata          - easy access to kdata structure.                *
 * _xtask_man_chan_setup_int - Set up the interrupt handler for async man chan*
 * _xtask_man_chan_int       - Interrupt handler for async man chan.          *
 * _xtask_kep                - Kernel proper.                                 *
 *                                                                            *
 ******************************************************************************/

#include <xs1.h>

/******************************************************************************
 * Macro:        SAVE_CONTEXT                                                 *
 *                                                                            *
 *               Save the context of the current runnig process on            *
 *               the stack of that process. The top word of the stack is      *
 *               not used. Note: it might be possible to use the first        *
 *               stack word (sp[0]) decreasing the context stack frame to     *
 *               19 stack words.                                              *
 ******************************************************************************/
#define SAVE_CONTEXT                                                                         \
    extsp   20;             /* make room on stack to save context */                         \
    stw     spc,    sp[1];  /* save the saved program counter register (must be sp[1]!) */   \
    stw     ssr,    sp[2];  /* save the saved status register (must be sp[2]!) */            \
    stw     sed,    sp[3];  /* save the saved exception data register (must be sp[3]!) */    \
    stw     et,     sp[4];  /* save the event type register (must be sp[4]!) */              \
    stw     dp,     sp[5];  /* save the data pointer */                                      \
    stw     cp,     sp[6];  /* save the constant pool pointer */                             \
    stw     lr,     sp[7];  /* save the link register */                                     \
    stw     r0,     sp[8];  /* save the general purpose registers r0-r11 */                  \
    stw     r1,     sp[9];                                                                   \
    stw     r2,     sp[10];                                                                  \
    stw     r3,     sp[11];                                                                  \
    stw     r4,     sp[12];                                                                  \
    stw     r5,     sp[13];                                                                  \
    stw     r6,     sp[14];                                                                  \
    stw     r7,     sp[15];                                                                  \
    stw     r8,     sp[16];                                                                  \
    stw     r9,     sp[17];                                                                  \
    stw     r10,    sp[18];                                                                  \
    stw     r11,    sp[19];                                                                  \
    ldaw    r10,    sp[0];  /* get value of current task stack pointer */                    \
    kentsp  0;              /* switch to kernel stack pointer */                             \
    ldw     r11,    sp[1];  /* address of kdata */                                           \
    ldw     r11,    r11[0]; /* address of task_entry */                                      \
    stw     r10,    r11[0]; /* save current task stack pointer to kdata->current_task->sp */ \
    krestsp 0               /* switch back to regular stack */

/******************************************************************************
 * Macro:        RESTORE_CONTEXT                                              *
 *                                                                            *
 *               Restore the task at which kdata->current_task points.        *
 *               Only a kret instruction is needed after executing this macro *
 *               to resume execution of this process.                         *
 *                                                                            *
 *               This macro expects a pointer to the kdata in r11             *
 *               prior to execution.                                          *
 ******************************************************************************/
#define RESTORE_CONTEXT                                                                      \
    ldw     r11,    r11[0];        /* load value of kdata->current_task pointer in r11 */    \
    ldw     r11,    r11[0];        /* load value of kdata->current_task->sp pointer in r11 */\
    set     sp,     r11;           /* set the SP to the SP of the process we are restoring */\
    ldw     spc,    sp[1];         /* restore saved program counter */                       \
    ldw     ssr,    sp[2];         /* restore saved status register */                       \
    ldw     sed,    sp[3];         /* restore saved exception data */                        \
    ldw     et,     sp[4];         /* restore exception type */                              \
    ldw     dp,     sp[5];         /* restore data pointer */                                \
    ldw     cp,     sp[6];         /* restore constant pool pointer */                       \
    ldw     lr,     sp[7];         /* restore link register */                               \
    ldw     r0,     sp[8];         /* restore GP registers r0-r9 */                          \
    ldw     r1,     sp[9];                                                                   \
    ldw     r2,     sp[10];                                                                  \
    ldw     r3,     sp[11];                                                                  \
    ldw     r4,     sp[12];                                                                  \
    ldw     r5,     sp[13];                                                                  \
    ldw     r6,     sp[14];                                                                  \
    ldw     r7,     sp[15];                                                                  \
    ldw     r8,     sp[16];                                                                  \
    ldw     r9,     sp[17];                                                                  \
    ldc     r10,    80;            /* we are going to decrement SP with 20 words */          \
    add     r11,    r11,     r10;  /* r11 holds the SP, add 80 to it */                      \
    set     sp,     r11;           /* set the new SP */                                      \
    sub     r11,    r11,     8;    /* find the position of r10 relative to the new SP */     \
    ldw     r10,    r11[0];        /* restore r10 */                                         \
    ldw     r11,    r11[1]         /* restore r11 (one word up from r10) */


/******************************************************************************
 * Function:     _xtask_restore_context                                       *
 * Parameters:   none                                                         *
 * Return:       void, handles over the processor to the restored task.       *
 *                                                                            *
 *               Restores the context and handles over the processor to the   *
 *               task pointed by kdata->current_task.                         *
 *               Used to start the scheduler.                                 *
 ******************************************************************************/
.extern  _xtask_restore_context
.globl   _xtask_restore_context.nstackwords
.globl   _xtask_restore_context.maxthreads
.globl   _xtask_restore_context.maxtimers
.globl   _xtask_restore_context.maxchanends
.linkset _xtask_restore_context.nstackwords, 0
.linkset _xtask_restore_context.maxthreads,  0
.linkset _xtask_restore_context.maxtimers,   0
.linkset _xtask_restore_context.maxchanends, 0
.globl   _xtask_restore_context

.cc_top _xtask_restore_context.func, _xtask_restore_context

_xtask_restore_context:
    kentsp  0                // switch to kernel stack
    ldw     r11,    sp[1]    // load address of kdata structure in r11
    krestsp 0                // switch back to regular stack 

    RESTORE_CONTEXT          // restore process context
    
    kret                     // handle over the processor to that process
    retsp   0                // should not be reached
.cc_bottom _xtask_restore_context.func


/******************************************************************************
 * Function:     _xtask_timer_int                                             *
 * Parameters:   none                                                         *
 * Return:       void                                                         *
 *                                                                            *
 *               Clock interrupt handler.                                     *
 *               1. Saves the context of the current running task.            *
 *               2. Set up the timer for the next interrupt.                  *
 *               3. Increment tick and check for expired delays.              *
 *               4. Invoke task scheduler to pick next task to run.           *
 *               5. Restore context of the next running task.                 *
 ******************************************************************************/
.globl   _xtask_timer_int
.globl   _xtask_timer_int.nstackwords
.globl   _xtask_timer_int.maxthreads
.globl   _xtask_timer_int.maxtimers
.globl   _xtask_timer_int.maxchanends
.linkset _xtask_timer_int.nstackwords, 0
.linkset _xtask_timer_int.maxthreads,  0
.linkset _xtask_timer_int.maxtimers,   0
.linkset _xtask_timer_int.maxchanends, 0

.text
.cc_top _xtask_timer_int.func, _xtask_timer_int

_xtask_timer_int:

    SAVE_CONTEXT            // save context of current running task

    /* Set up the timer for the next interrupt
       The time of the current interrupt is
       saved in kdata->timer_int.
       We add the timer cycles of 1 tick
       to it and save it in the data
       register of the timer and in kdata->timer_int.
       This is done because the time at which
       the timer interrupt happened is not
       automatically saved by the processor */

    kentsp  1                         // switch to kernel stack, increase with 1 word
    ldw     r0,        sp[2]          // load address of kdata in r0 from kernel stack 

    ldc     r1,        36             // 9 words is 36 bytes
    add     r0,        r0,       r1   // r0 is now kdata address + 36 bytes
    ldw     r1,        r0[0]          // load kdata->timer_res in r1 (offset 9 words)   

    add     r0,        r0,       4    // r0 is now kdata address + 40 bytes
    ldw     r3,        r0[0]          // load kdata->timer_cycles in r3 (offset 10 words)
    
    add     r0,        r0,       4    // r0 is now kdata address + 44 bytes
    ldw     r2,        r0[0]          // load kdata->timer_int in r2 (offset 11 words)

    add    r2,         r2,       r3   // calculate the time for the next interrupt
    setd   res[r1],    r2             // set the timer value for the next interrupt
    
    stw    r2,         r0[0]          // store r2 to kdata->timer_int

    // increase ticks
    add    r0,         r0,       4    // r0 is now kdata address + 48 bytes
    ldw    r2,         r0[0]          // load kdata->time in r2 (offset 12 words)
    add    r2,         r2,       1    // increase with 1 tick
    stw    r2,         r0[0]          // store r2 back in kdata->time

    ldw    r0,         sp[2]          // load address of kdata in r0
    ldw    r1,         r0[0]          // load kdata->current_task pointer in r1

    bl     xtask_enqueue              // deschedule and enqueue current task

    ldw    r0,         sp[2]          // load address of kdata in r0
    bl     xtask_check_delayed_tasks  // check for expired timers

    ldw    r0,         sp[2]          // load address of kdata in r0
    ldc    r1,         0
    stw    r1,         r0[0]          // set kdata->current_task to 0 (NULL)
    bl     xtask_pick_task            // invoke task scheduler, it will update kdata->current_task

    ldw    r11,         sp[2]         // load address of kdata in r11
    
    krestsp 1                         // switch to regular stack, decrease kstack with 1 word

    RESTORE_CONTEXT                   // restore context of next running task
    kret                              // handle over the processor to the restored task

.cc_bottom _xtask_timer_int.func

/******************************************************************************
 * Function:     _xtask_init_system                                           *
 * Parameters:   none                                                         *
 * Return:       void                                                         *
 *                                                                            *
 *               Initialise some low level parts of the system:               *
 *               1. Allocate and set up the timer to generate interrupts.     *
 ******************************************************************************/
.extern  _xtask_init_system
.globl   _xtask_init_system.nstackwords
.globl   _xtask_init_system.maxthreads
.globl   _xtask_init_system.maxtimers
.globl   _xtask_init_system.maxchanends
.linkset _xtask_init_system.nstackwords, 0
.linkset _xtask_init_system.maxthreads,  0
.linkset _xtask_init_system.maxtimers,   1
.linkset _xtask_init_system.maxchanends, 0
.globl   _xtask_init_system

.cc_top _xtask_init_system.func, _xtask_init_system

_xtask_init_system:
 
    getr    r1,         1                            // get a timer resource
    kentsp  0                                        // switch to kernel stack
    ldw     r3,         sp[1]                        // load address of kdata from kernel stack in r3
    krestsp 0                                        // switch back to regular stack
    stw     r1,         r3[9]                        // store timer resource in kdata->timer_res (offset 9 words)
    
    // setup tick timer
    ldap    r11,        _xtask_timer_int             // load address of interrupt handler in r11
    setv    res[r1],    r11                          // set the interrupt handler of the timer
    setc    res[r1],    XS1_SETC_IE_MODE_INTERRUPT   // configure timer to interrupt mode
    setc    res[r1],    XS1_SETC_COND_AFTER          // generate interrupt if timer value > timer data register
    in      r0,         res[r1]                      // get current timer value
    
    ldw     r2,         r3[10]                       // load kdata->timer_cycles in r2 (offset 10 words)
    add     r0,         r0,              r2          // add to current timer value the amount of cycles for 1 tick
    setd    res[r1],    r0                           // save the timer value of next interrupt in data register of timer
    
    stw     r0,         r3[11]                       // save timer value of next int to kdata->timer_int (offset 11 words)

    eeu     res[r1]                                  // enable events and interrupts from timer

    setsr   0x02  // set status register to enable interrupts
                  // If using XGDB and breakpoints before starting the scheduler,
                  // enable interrupts at _xtask_restore_context before the kret instruction.
    retsp   0

.cc_bottom _xtask_init_system.func

/******************************************************************************
 * Function:     _xtask_init_kdata                                            *
 * Parameters:   r0 - pointer to bottom of kernel stack                       *
 *               r1 - intended offset                                         *
 *               r3 - address of kdata                                        *
 * Return:       void                                                         *
 *                                                                            *
 *               1. Set up kernel stack                                       *
 *               2. push address of kernel data structure on kernel stack     *
 *               3. Set kernel entry point                                    *
 ******************************************************************************/
.extern  _xtask_init_kdata
.globl   _xtask_init_kdata.nstackwords
.globl   _xtask_init_kdata.maxthreads
.globl   _xtask_init_kdata.maxtimers
.globl   _xtask_init_kdata.maxchanends
.linkset _xtask_init_kdata.nstackwords, 0
.linkset _xtask_init_kdata.maxthreads,  0
.linkset _xtask_init_kdata.maxtimers,   0
.linkset _xtask_init_kdata.maxchanends, 0
.globl   _xtask_init_kdata
.cc_top  _xtask_init_kdata.func, _xtask_init_kdata
_xtask_init_kdata:
    
    // set up kernel stack
    ldaw     r3,   sp[0]        // save current stack pointer to r3
    add      r0,   r0,    r1    // add offset to top of kernel stack
    set      sp,   r0           // set SP to value of kernel stack pointer
    krestsp  0                  // save it to kernel stack pointer
    set      sp,   r3           // restore stack pointer

    stw      r2,   r0[1]        // save address of kdata structure to bottom of stack (1 word above current kernel stack pointer)

    ldap     r11,  _xtask_kep   // load address of kernel entry point to r11
    set      kep,  r11          // set kernel entry point

    retsp 0

.cc_bottom _xtask_init_kdata.func

/******************************************************************************
 * Function:     _xtask_init_task_stack                                       *
 * Parameters:   r0 - pointer to stack to initialise                          *
 *               r1 - pointer to task code                                    *
 *               r2 - pointer to parameters for task                          *
 * Return:       value of stackpointer of process after initialisation        *
 *                                                                            *
 *               Initializes the stack of a newly created task in such a way  *
 *               as if it was already running. A context switch by the        *
 *               scheduler will start the process.                            *
 ******************************************************************************/
.extern  _xtask_init_task_stack
.globl   _xtask_init_task_stack.nstackwords
.globl   _xtask_init_task_stack.maxthreads
.globl   _xtask_init_task_stack.maxtimers
.globl   _xtask_init_task_stack.maxchanends
.linkset _xtask_init_task_stack.nstackwords, 0
.linkset _xtask_init_task_stack.maxthreads,  0
.linkset _xtask_init_task_stack.maxtimers,   0
.linkset _xtask_init_task_stack.maxchanends, 0
.globl   _xtask_init_task_stack,"f{p(sl)}(p(0),p(f{0}(p(0))),p(0))"
.cc_top  _xtask_init_task_stack.func, _xtask_init_task_stack

_xtask_init_task_stack:
    ldaw      r3,       sp[0]    // save current SP to r3, need to restore SP when leaving function
    set       sp,       r0       // set current SP to the first parameter
    extsp     22                 // extend stack with 22 words

    stw       r1,       sp[1]    // save pointer to task on stack (PC)
    
    ldc       r0,       0x02     // save status register on stack, 0x02 = enable interrupts
    stw       r0,       sp[2]     

    stw       sed,      sp[3]    // save saved exception data on stack, inherit from calling process
    stw       et,       sp[4]    // save exception type on stack, inherit from calling process

    ldaw      r0,       dp[0]    // save data pointer on stack, inherit from calling process
    stw       r0,       sp[5]
    
    ldaw      r11,      cp[0]    // save constant pointer on stack, inherit from calling process
    stw       r11,      sp[6]

    ldc       r0,       0        // save link register on stack, init with 0
    stw       r0,       sp[7]

    stw       r2,       sp[8]    // register r0 - pointer to parameters (r2)

    ldc       r0,       1        // r1
    stw       r0,       sp[9]
    
    ldc       r0,       2        // r2
    stw       r0,       sp[10]   

    ldc       r0,       3        // r3
    stw       r0,       sp[11]

    ldc       r0,       4        // r4
    stw       r0,       sp[12]

    ldc       r0,       5        // r5
    stw       r0,       sp[13]

    ldc       r0,       6        // r6
    stw       r0,       sp[14]

    ldc       r0,       7        // r7
    stw       r0,       sp[15]

    ldc       r0,       8        // r8
    stw       r0,       sp[16]

    ldc       r0,       9        // r9
    stw       r0,       sp[17]

    ldc       r0,       10       // r10
    stw       r0,       sp[18]

    ldc       r0,       11       // r11
    stw       r0,       sp[19]

    ldc       r0,       0xdead   // two test values at the start of the stack for debugging
    stw       r0,       sp[20]
    
    ldc       r0,       0xbabe
    stw       r0,       sp[21]

    ldaw      r0,       sp[0]    // new task SP as return value
    
    set       sp,       r3       // restore stack pointer to calling task stack pointer

    retsp     0
    
.cc_bottom _xtask_init_task_stack.func

// temporary function for easy access to kdata
.extern  _xtask_get_kdata
.globl   _xtask_get_kdata.nstackwords
.globl   _xtask_get_kdata.maxthreads
.globl   _xtask_get_kdata.maxtimers
.globl   _xtask_get_kdata.maxchanends
.linkset _xtask_get_kdata.nstackwords, 0
.linkset _xtask_get_kdata.maxthreads,  0
.linkset _xtask_get_kdata.maxtimers,   0
.linkset _xtask_get_kdata.maxchanends, 0
.globl   _xtask_get_kdata,"f{p(0)}()"
.cc_top  _xtask_get_kdata.func, _xtask_get_kdata

_xtask_get_kdata:
    kentsp   0          
    ldw      r0,   sp[1]          // load address of kdata in r0
    krestsp  0
    retsp    0

.cc_bottom _xtask_get_kdata.func

/******************************************************************************
 * Function:     _xtask_man_chan_setup_int                                    *
 * Parameters:   r0 - chanend resource                                        *
 *               r1 - environment (pointer to kdata)                          *
 * Return:       void                                                         *
 *                                                                            *
 *               Sets up the interrupt handler for the management channel.    *
 ******************************************************************************/
.extern  _xtask_man_chan_setup_int
.globl   _xtask_man_chan_setup_int
.globl   _xtask_man_chan_setup_int.nstackwords
.globl   _xtask_man_chan_setup_int.maxthreads
.globl   _xtask_man_chan_setup_int.maxtimers
.globl   _xtask_man_chan_setup_int.maxchanends
.linkset _xtask_man_chan_setup_int.nstackwords, 0
.linkset _xtask_man_chan_setup_int.maxthreads,  0
.linkset _xtask_man_chan_setup_int.maxtimers,   0
.linkset _xtask_man_chan_setup_int.maxchanends, 0
.globl   _xtask_man_chan_setup_int,"f{0}(ui,p(0))"
.cc_top  _xtask_man_chan_setup_int.func, _xtask_man_chan_setup_int

_xtask_man_chan_setup_int:

    ldap      r11,         _xtask_man_chan_int         // load address of interrupt handler in r11
    setv      res[r0],     r11                         // set interrupt handler
    setc      res[r0],     XS1_SETC_IE_MODE_INTERRUPT  // configure chanend to interrupt mode
    add       r11,         r1,       0                 // copy r1 to r11
    setev     res[r0],     r11                         // set chanend environment vector
    eeu       res[r0]                                  // enable events and interrupts on chanend
    retsp     0

.cc_bottom _xtask_man_chan_setup_int.func

/******************************************************************************
 * Function:     _xtask_man_chan_int                                          *
 * Parameters:   ed - address of kdata structure,                             *
 *                    loaded from chanend environment vector on interrupt.    *
 * Return:       void                                                         *
 *                                                                            *
 *               Interrupt handler for the management channel.                *
 ******************************************************************************/
.extern  _xtask_man_chan_int
.globl   _xtask_man_chan_int
.globl   _xtask_man_chan_int.nstackwords
.globl   _xtask_man_chan_int.maxthreads
.globl   _xtask_man_chan_int.maxtimers
.globl   _xtask_man_chan_int.maxchanends
.linkset _xtask_man_chan_int.nstackwords, 0
.linkset _xtask_man_chan_int.maxthreads,  0
.linkset _xtask_man_chan_int.maxtimers,   0
.linkset _xtask_man_chan_int.maxchanends, 0
.cc_top  _xtask_man_chan_int.func, _xtask_man_chan_int

_xtask_man_chan_int:

    SAVE_CONTEXT                       // save context of interrupted task

    kentsp    1                        // switch to kernel stack, increase with 1 word
    get       r11,       ed            // load kdata address saved in ed to r11
    add       r0,        r11,   0      // copy r11 to r0
    bl        xtask_get_not_chan       // get notification chanend resource in r0
    
    chkct     res[r0],   0x1           // receive and check control token 0x01

    ldw       r0,        sp[2]         // load address of kdata in r0
    bl        xtask_not_handler        // call notifcation handler

    ldw       r11,       sp[2]         // load address of kdata in r0
    krestsp   1                        // switch back to regular stack, decrease kstack with 1 word
    
    RESTORE_CONTEXT                    // restore the next running task
    kret                               // handle over the processor to the next task

.cc_bottom _xtask_man_chan_int.func

/********************************************************************************
 * KERNEL ENTRY POINT                                                           *
 *                                                                              *
 * Exceptions jump to the kernel entry point (infinite loop)                    *
 * Kernel calls jump to the kernel entry point + 64 bytes.                      *
 ********************************************************************************/ 
.extern  _xtask_kep
.globl   _xtask_kep
.globl   _xtask_kep.nstackwords
.globl   _xtask_kep.maxthreads
.globl   _xtask_kep.maxtimers
.globl   _xtask_kep.maxchanends
.linkset _xtask_kep.nstackwords, 0
.linkset _xtask_kep.maxthreads,  0
.linkset _xtask_kep.maxtimers,   0
.linkset _xtask_kep.maxchanends, 0
.cc_top  _xtask_kep.func, _xtask_kep

.align 128                            // align the exception section to 128 bytes
_xtask_kep:                           // entry point for exceptions
    bu        _xtask_kep              // infinite loop

.align 64                             // kernel must be aligned on 64 bytes
xtask_kcep:                           // entry point for kernel calls (switch context)

    SAVE_CONTEXT                      // save context of calling task

    ldw       r0,          sp[1]      // the saved program counter does not contain the next instruction
    ldc       r1,          2          // but the address of the KCALL instruction.
    add       r0,          r0,    r1  // Add two to the saved program counter that was saved on the stack
    stw       r0,          sp[1]      // to jump over the KCALL instruction.

    get       r11,         ed         // ed contains the kernel call number, copy to r11
    add       r0,          r11,   0   // copy again to r0
    ldw       r2,          sp[8]      // r0 of saved task, pointer to struct kcall_data

    kentsp    1                       // switch to kernel stack
    ldw       r1,          sp[2]      // load address of kdata in r1

                                      // r0 = kcall number, r1 = kdata address, r2 = kcall_data address
    bl        xtask_kcall_handler     // call the kernel call handler


    ldw       r11,         sp[2]      // load address of kdata in r11
    krestsp   1                       // switch to regular stack, decrease kstack with 1 word

    RESTORE_CONTEXT                   // restore context of the next running task
    kret                              // handle over the processor to this task

.cc_bottom _xtask_kep.func

/* END OF KERNEL */
