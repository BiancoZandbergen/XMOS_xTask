#ifndef TILE_H
#define TILE_H

/* deal with single tile targets */

#if defined(XC_1)
#define AP_TILE_0 0
#define AP_TILE_1 1
#elif defined(XC_1A)
#define AP_TILE_0 0
#define AP_TILE_1 1
#elif defined(XC_2)
#define AP_TILE_0 0
#define AP_TILE_1 1
#elif defined(XC_3)
#define AP_TILE_0 0
#define AP_TILE_1 1
#elif defined(XC_5)
#define AP_TILE_0 0
#define AP_TILE_1 0
#elif defined(XK_1)
#define AP_TILE_0 0
#define AP_TILE_1 0
#elif defined(XK_1A)
#define AP_TILE_0 0
#define AP_TILE_1 0
#elif defined(XDK)
#define AP_TILE_0 0
#define AP_TILE_1 1
#elif defined(STARTKIT)
#define AP_TILE_0 0
#define AP_TILE_1 0
#else
#endif

#endif /* TILE_H */