#ifndef __EPD_CLOCK_DEMO_DIGITS_H__
#define __EPD_CLOCK_DEMO_DIGITS_H__

/* Reuse the reference demo digit bitmaps locally so the clock partial refresh
 * follows the same 32x64 block update style as the sample project. */
#define Num EPD_DEMO_Num
#define gImage_numdot EPD_DEMO_NumDot
#define gImage_white EPD_DEMO_White
#include "../../../S-GDEM0122T61/S-GDEM0122T61-P-20241016/HARDWARE/EPD/Ap_29demo.h"
#undef gImage_white
#undef gImage_numdot
#undef Num

#endif /* __EPD_CLOCK_DEMO_DIGITS_H__ */
