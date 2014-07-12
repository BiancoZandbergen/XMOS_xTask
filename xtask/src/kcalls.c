/******************************************************************************
 *                                                                            *
 * File:   kcalls.c                                                           *
 * Author: Bianco Zandbergen <bianco [AT] zandbergen.name>                    *
 *                                                                            *
 * This file is part of the xTask Distributed Operating System for            *
 * the XMOS XS1 microprocessor architecture (www.xtask.org).                  *
 *                                                                            *
 * This file contains the majority of the API functions.                      *
 * More specific it contains the following functions:                         *
 *                                                                            *
 * xtask_delay_ticks          - delay task for number of kernel ticks         *
 * xtask_create_thread        - create new (same tile) ded. hardware thread   *
 * xtask_vc_receive           - receive from virtual channel                  *
 * xtask_vc_get_write_buf     - get virtual channel write buffer              *
 * xtask_vc_send              - send virtual channel write buffer             *
 * xtask_create_mailbox       - register mailbox for inter-task communication *
 * xtask_create_remote_thread - create new (other tile) ded. hardware thread  *
 * xtask_get_outbox           - get mailbox outbox buffer                     *
 * xtask_send_outbox          - send outbox to recipient task                 *
 * xtask_get_inbox            - receive a message from another task           *
 * xtask_create_task          - create a new task                             *
 *                                                                            *
 ******************************************************************************/

#include <stdio.h>
#include <xccompat.h>
#include "../include/kernel.h"
#include "../include/comserver.h"

/******************************************************************************
 * Function:     xtask_delay_ticks                                            *
 * Parameters:   ticks      - Number of kernel ticks that this task should    *
 *                            delay.                                          *
 * Return:       void                                                         *
 *                                                                            *
 *               Delay for a certain number of kernel ticks.                  *
 ******************************************************************************/
void xtask_delay_ticks(unsigned int ticks)
{
  struct kcall_data kcall_params;
  struct kcall_data *p = &kcall_params;
  kcall_params.p0 = ticks;

  __asm__ volatile ("add r0, %0, 0"::"r"(p));
  asm("kcall 1");
}

/******************************************************************************
 * Function:     xtask_create_thread                                          *
 * Parameters:   pc           - Function pointer to function that this        *
 *                              hardware thread will execute.                 *
 *                              The function must return void and accept      *
 *                              a void pointer (args) and a chanend as        *
 *                              parameters.                                   *
 *               stackwords   - Stack size in words.                          *
 *               args         - Pointer to argument buffer (or use the        *
 *                              pointer value itself as parameter).           *
 *               obj_size     - The size of objects transferred through the   *
 *                              channel.                                      *
 *               rx_buf_size  - Task receive buffer size (must be multiple of *
 *                              object size).                                 *
 *               tx_buf_Size  - Task transfer buffer size (must be multiple   *
 *                              of object size).                              *
 * Return:       handle                                                       *
 *                                                                            *
 *               Create a new dedicated hardware thread (local, same tile)    *
 ******************************************************************************/
unsigned int xtask_create_thread(hwt_code     pc, 
                                 unsigned int stackwords, 
                                 void *       args, 
                                 unsigned int obj_size, 
                                 unsigned int rx_buf_size, 
                                 unsigned int tx_buf_size)
{
  struct kcall_data kcall_params;
  struct kcall_data *p = &kcall_params;
  kcall_params.p0 = (unsigned int) pc;
  kcall_params.p1 = (unsigned int) stackwords;
  kcall_params.p2 = (unsigned int) args;
  kcall_params.p3 = (unsigned int) obj_size;
  kcall_params.p4 = (unsigned int) rx_buf_size;
  kcall_params.p5 = (unsigned int) tx_buf_size;

  __asm__ volatile ("add r0, %0, 0"::"r"(p));
  __asm__ volatile ("kcall 2");

  return kcall_params.p0; 
}

/******************************************************************************
 * Function:     xtask_vc_receive                                             *
 * Parameters:   handle           - Handle to dedicated hardware thread       *
 *               min_size         - Minimum amount of data to receive in      *
 *                                  bytes. If set to 0, the minimum amount    *
 *                                  is a full buffer.                         *
 * Return:       Pointer to a vc_buf struct which contains all the            *
 *               information about the buffer such as the actual pointer to   *
 *               the buffer and the amount of data in the buffer.             *
 *                                                                            *
 *               Receive from a hardware thread through a virtual channel.    *
 *               The task will block if there is no (sufficient) data         *
 *               available.                                                   *
 ******************************************************************************/
struct vc_buf * xtask_vc_receive(unsigned int handle, 
                                 unsigned int min_size)
{
  struct kcall_data kcall_params;
  struct kcall_data *p = &kcall_params;
  kcall_params.p0 = handle;
  kcall_params.p1 = min_size;
  
  __asm__ volatile ("add r0, %0, 0"::"r"(p));
  __asm__ volatile ("kcall 3");

  return (struct vc_buf *)kcall_params.p0;  
}

/******************************************************************************
 * Function:     xtask_vc_get_write_buf                                       *
 * Parameters:   handle   - Handle to dedicated hardware thread               *
 * Return:       A pointer to a vc_buf struct that contains the information   *
 *               about the buffer that can be filled by the task and          *
 *               transmitted to the dedicated hardware thread.                *
 *               Null pointer when no buffer was available but this should    *
 *               not happen in regular operation.                             *
 *                                                                            *
 *               Receive a write buffer that can be filled by the task and    *
 *               transmitted to the dedicated hardware thread. This function  *
 *               should only be called once prior to the first transmission   *
 *               to the dedicated hardware thread. The function that sends    *
 *               the buffer to the dedicated hardware thread will return a    *
 *               new empty buffer.                                            *
 ******************************************************************************/
struct vc_buf * xtask_vc_get_write_buf(unsigned int handle)
{
  struct kcall_data kcall_params;
  struct kcall_data *p = &kcall_params;
  kcall_params.p0 = handle;

  __asm__ volatile ("add r0, %0, 0"::"r"(p));
  __asm__ volatile ("kcall 4");

  return (struct vc_buf *)kcall_params.p0;
}

/******************************************************************************
 * Function:     xtask_vc_send                                                *
 * Parameters:   buf   - Pointer to struct vc_buf.                            *
 * Return:       A pointer to a vc_buf struct that contains the information   *
 *               about the buffer that can be filled by the task and          *
 *               transmitted to the dedicated hardware thread.                *
 *               Null pointer when no buffer was available but this should    *
 *               not happen in regular operation.                             *
 *                                                                            *
 *               Instruct the Communication Server to send the write buffer   *
 *               to the dedicated hardware thread. Receive a new empty        *
 *               write buffer that can be immediately filled by the task.     *
 ******************************************************************************/
struct vc_buf * xtask_vc_send(struct vc_buf *buf)
{
  struct kcall_data kcall_params;
  struct kcall_data *p = &kcall_params;
  kcall_params.p0 = (unsigned int)buf;

  __asm__ volatile ("add r0, %0, 0"::"r"(p));
  __asm__ volatile ("kcall 5");

  return (struct vc_buf *)kcall_params.p0;
}

/******************************************************************************
 * Function:     xtask_create_mailbox                                         *
 * Parameters:   id          - ID for the mailbox to create.                  *
 *                             Must be unique system wide.                    *
 *               inbox_size  - Inbox size in bytes.                           *
 *               outbox_size - Outbox size in bytes.                          *
 * Return:       Always returns 1 currently.                                  *
 *                                                                            *
 *               Create a new mailbox for inter-task communication.           *
 ******************************************************************************/
unsigned int xtask_create_mailbox(unsigned int id, 
                                  unsigned int inbox_size, 
                                  unsigned int outbox_size)
{
  struct kcall_data kcall_params;
  struct kcall_data *p = &kcall_params;
  kcall_params.p0 = id;
  kcall_params.p1 = inbox_size;
  kcall_params.p2 = outbox_size;
  
  __asm__ volatile ("add r0, %0, 0"::"r"(p));
  __asm__ volatile ("kcall 6");

  return kcall_params.p0;
}

/******************************************************************************
 * Function:     xtask_create_remote_thread                                   *
 * Parameters:   code         - The function number (not a function pointer)  *
 *                              that will be executed by this hardware thread.*
 *                              The code of the function should already be on *
 *                              the tile that starts the hardware thread.     *
 *               stackwords   - Stack size in words.                          *
 *               obj_size     - The size of objects transferred through the   *
 *                              channel (must be a multiple of four bytes)    *
 *               rx_buf_size  - Task receive buffer size (must be multiple of *
 *                              object size).                                 *
 *               tx_buf_Size  - Task transfer buffer size (must be multiple   *
 *                              of object size).                              *
 * Return:       handle                                                       *
 *                                                                            *
 *               Create a new dedicated hardware thread (remote, different    *
 *               tile) This function is highly expirimental.                  *
 ******************************************************************************/
unsigned int xtask_create_remote_thread(unsigned int code,
                                        unsigned int stackwords,
                                        unsigned int obj_size,
                                        unsigned int rx_buf_size,
                                        unsigned int tx_buf_size)
{
  struct kcall_data kcall_params;
  struct kcall_data *p = &kcall_params;
  kcall_params.p0 = (unsigned int) code;
  kcall_params.p1 = (unsigned int) stackwords;
  kcall_params.p2 = (unsigned int) obj_size;
  kcall_params.p3 = (unsigned int) rx_buf_size;
  kcall_params.p4 = (unsigned int) tx_buf_size;

  __asm__ volatile ("add r0, %0, 0"::"r"(p));
  __asm__ volatile ("kcall 7");

  return kcall_params.p0; 
}

/******************************************************************************
 * Function:     xtask_get_outbox                                             *
 * Parameters:   id          - Mailbox id.                                    *
 * Return:       A pointer to a vc_buf struct that contains the information   *
 *               about the buffer that can be filled by the sending task and  *
 *               transmitted to the recipient task.                           *
 *                                                                            *
 *               Get access to the outbox buffer of a mailbox.                *
 ******************************************************************************/
struct vc_buf * xtask_get_outbox(unsigned int id)
{
  struct kcall_data kcall_params;
  struct kcall_data *p = &kcall_params;
  kcall_params.p0 = id;
  
  __asm__ volatile ("add r0, %0, 0"::"r"(p));
  __asm__ volatile ("kcall 8");

  return (struct vc_buf *) kcall_params.p0; 
}

/******************************************************************************
 * Function:     xtask_send_outbox                                            *
 * Parameters:   sender    - sender mailbox id                                *
 *               receiver  - recipient mailbox id                             *
 * Return:       Returns 0 when the message has been delivered. 1 when the    *
 *               recipient could not be found.                                * 
 *                                                                            *
 *               Send outbox to recipient task. The sending task will be      *
 *               blocked until the recipient task has actively received the   *
 *               message. The recipient task can be on the same kernel,       *
 *               on the same tile or on a different tile.                     *
 ******************************************************************************/
unsigned int xtask_send_outbox(unsigned int sender, 
                               unsigned int receiver)
{
  struct kcall_data kcall_params;
  struct kcall_data *p = &kcall_params;
  kcall_params.p0 = sender;
  kcall_params.p1 = receiver;
  
  __asm__ volatile ("add r0, %0, 0"::"r"(p));
  __asm__ volatile ("kcall 9");

  return kcall_params.p0; 
}

/******************************************************************************
 * Function:     xtask_get_inbox                                              *
 * Parameters:   id       - mailbox id                                        *
 *               location - Check for pending senders on local                *
 *                          CS (ITC_LOCAL) or everywhere (ITC_ANYWHERE).      *
 * Return:       Pointer to a vc_buf struct which contains all the            *
 *               information about the buffer such as the actual pointer to   *
 *               the buffer and the amount of data in the buffer.             *
 *                                                                            *
 *               Receive a message from another task. The calling task will   *
 *               be blocked until there is a message available.               *
 ******************************************************************************/
struct vc_buf * xtask_get_inbox(unsigned int id, 
                                unsigned int location)
{
  struct kcall_data kcall_params;
  struct kcall_data *p = &kcall_params;
  kcall_params.p0 = id;
  kcall_params.p1 = location;
  
  __asm__ volatile ("add r0, %0, 0"::"r"(p));
  __asm__ volatile ("kcall 10");

  return (struct vc_buf *) kcall_params.p0; 
}

/******************************************************************************
 * Function:     xtask_create_task                                            *
 * Parameters:   code         - Pointer to the function that the task will    *
 *                              execute. This function must return void and   *
 *                              accept a void * as argument                   *
 *               stack_size   - Stack size in words                           *
 *               priority     - Task priority, 0-6, lower number is higher    *
 *                              priority                                      *
 *               tid          - Task id, must be unique                       *
 *               args         - Pointer to argument buffer (or use the        *
 *                              pointer value itself as argument)             *
 * Return:       Currently always returns 1                                   *
 *                                                                            *
 *               Create a new task at run time by another task.               *
 ******************************************************************************/
int xtask_create_task(task_code    code, 
                      unsigned int stack_size, 
                      unsigned int priority, 
                      unsigned int tid, 
                      void *       args)
{
  struct kcall_data kcall_params;
  struct kcall_data *p = &kcall_params;
  
  kcall_params.p0 = (unsigned int) code;
  kcall_params.p1 = stack_size;
  kcall_params.p2 = priority;
  kcall_params.p3 = tid;
  kcall_params.p4 = (unsigned int) args;
  
  __asm__ volatile ("add r0, %0, 0"::"r"(p));
  __asm__ volatile ("kcall 11");

  return (int)kcall_params.p0;  
}
