/*
  * Author: Anastasis Vagenas
  * Contact: anasvag29@gmail.com
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __PCD_8544_FONT_H
#define __PCD_8544_FONT_H

#include <stdint.h>

/* Font types */
#define SMALL_FONT      0x10
#define MEDIUM_FONT     0x20
#define LARGE_FONT      0x40
#define FONT_MASK       0xf0

/* Font alignment */
#define ALIGN_UP            0x00
#define ALIGN_CENTER        0x01
#define ALIGN_BOTTOM        0x02
#define ALIGMENT_MASK       0x03

/* PCD8544 Different fonts */
extern const uint8_t small_font[];
extern const uint8_t medium_font[];
extern const uint8_t large_font[];

/* TODO - Still not used */
extern const uint8_t MediumNumbers[];
extern const uint8_t BigNumbers[];

#endif /* __PCD_8544_FONT_H */
