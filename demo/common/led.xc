/******************************************************************************
 *                                                                            *
 * File:   led.xc                                                             *
 * Author: Bianco Zandbergen <bianco [AT] zandbergen.name>                    *
 *                                                                            *
 * Toggle led(s) based on the specific hardware target.                       *
 *                                                                            *
 ******************************************************************************/
#include <platform.h>
#include "tile.h"

#if defined(XC_1)
on tile[AP_TILE_0] : out port leds = XS1_PORT_8D;
unsigned long led_mask_set         = 0xF0;
unsigned long led_mask_clr         = 0x00;
#elif defined(XC_1A)
on tile[AP_TILE_0] : out port leds = XS1_PORT_4C;
unsigned long led_mask_set         = 0x0F;
unsigned long led_mask_clr         = 0x00;
#elif defined(XC_2)
on tile[AP_TILE_0] : out port leds = XS1_PORT_1E;
unsigned long led_mask_set         = 0x01;
unsigned long led_mask_clr         = 0x00;
#elif defined(XC_3)
on tile[AP_TILE_0] : out port leds =
unsigned long led_mask_set         =
unsigned long led_mask_clr         =
#elif defined(XC_5)
on tile[AP_TILE_0] : out port leds = XS1_PORT_4E;
unsigned long led_mask_set         = 0x0F;
unsigned long led_mask_clr         = 0x00;
#elif defined(XK_1)
on tile[AP_TILE_0] : out port leds = XS1_PORT_4F;
unsigned long led_mask_set         = 0x0F;
unsigned long led_mask_clr         = 0x00;
#elif defined(XK_1A)
on tile[AP_TILE_0] : out port leds = XS1_PORT_4F;
unsigned long led_mask_set         = 0x0F;
unsigned long led_mask_clr         = 0x00;
#elif defined(XDK)
on tile[AP_TILE_0] : out port leds = XS1_PORT_1F;
unsigned long led_mask_set         = 0x01;
 unsigned long led_mask_clr        = 0x00;
#elif defined(STARTKIT)
on tile[AP_TILE_0] : out port leds = XS1_PORT_1A;
unsigned long led_mask_set         = 0x01;
unsigned long led_mask_clr         = 0x00;
#else
on tile[AP_TILE_0] : out port leds =
unsigned long led_mask_set         = 
unsigned long led_mask_clr         =
#endif

void set_leds()
{
  leds <: led_mask_set;  
}

void clr_leds()
{
  leds <: led_mask_clr;  
}

