/******************************************************************************
 *                                                                            *
 * File:   main.xc                                                            *
 * Author: Bianco Zandbergen <bianco [AT] zandbergen.name>                    *
 *                                                                            *
 * Main program for demo.                                                     *
 * This demo makes use of print statements as output.                         *  
 *                                                                            *
 ******************************************************************************/
#include <platform.h>
#include <stdio.h>
#include <xccompat.h>
#include "../../xtask/include/xtask.h"
#include "../common/led.h"
#include "../common/tile.h"

void start_kernel_0(chanend r, chanend w);
void start_kernel_1(chanend r, chanend w);

void infinite_receive(chanend c)
{
  unsigned int i;
  
  while (1) {
    c :> i;
    printf("received from task: %u\n", i);
  }
}

int main(void)
{

  /* management and notification channels for communication 
     between kernels and communication servers */
  chan c0_man[1];
  chan c0_not[1];
  
  chan c1_man[1];
  chan c1_not[1];
  
  /* ring bus channels to interconnect communication servers */
  chan ring[2];
  
  par {
    
    /* start communication servers on tile 0 and 1 */
    on tile[AP_TILE_0] : xtask_comserver(c0_not, c0_man, 1, ring[0], ring[1], 1);
    on tile[AP_TILE_1] : xtask_comserver(c1_not, c1_man, 1, ring[1], ring[0], 2);

    /* start kernels on tile 0 and 1 */
    on tile[AP_TILE_0] : start_kernel_0(c0_man[0], c0_not[0]);
    on tile[AP_TILE_1] : start_kernel_1(c1_man[0], c1_not[0]);
  }

  return 0;
}

