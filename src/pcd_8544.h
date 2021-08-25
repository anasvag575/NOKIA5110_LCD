/*
  * Author: Anastasis Vagenas
  * Contact: anasvag29@gmail.com
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __PCD_8544_H
#define __PCD_8544_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes */
#include <stdbool.h>
#include <pcd_8544_font.h>
#include "stm32f4xx_hal.h"

#define LCDWIDTH 84     ///< LCD is 84 pixels wide
#define LCDHEIGHT 48    ///< 48 pixels high
#define LCDBUFFER_SZ   (LCDWIDTH * LCDHEIGHT / 8)

#define PCD8544_POWERDOWN           0x04    ///< Function set, Power down mode
#define PCD8544_ENTRYMODE           0x02    ///< Function set, Entry mode
#define PCD8544_EXTENDEDINSTRUCTION 0x01    ///< Function set, Extended instruction set control

#define PCD8544_DISPLAYBLANK    0x0         ///< Display control, blank
#define PCD8544_DISPLAYNORMAL   0x4         ///< Display control, normal mode
#define PCD8544_DISPLAYALLON    0x1         ///< Display control, all segments on
#define PCD8544_DISPLAYINVERTED 0x5         ///< Display control, inverse mode

#define PCD8544_FUNCTIONSET     0x20        ///< Basic instruction set
#define PCD8544_DISPLAYCONTROL  0x08        ///< Basic instruction set - Set display configuration
#define PCD8544_SETYADDR        0x40        ///< Basic instruction set - Set Y address of RAM, 0 <= Y <= 5
#define PCD8544_SETXADDR        0x80        ///< Basic instruction set - Set X address of RAM, 0 <= X <= 83

#define PCD8544_SETTEMP 0x04                ///< Extended instruction set - Set temperature coefficient
#define PCD8544_SETBIAS 0x10                ///< Extended instruction set - Set bias system
#define PCD8544_SETVOP  0x80                ///< Extended instruction set - Write Vop to register

#define PCD8544_BIAS_DEFAULT    0x00    // 0x07 max
#define PCD8544_VOP_DEFAULT     80     // 0x7f max

/* Timeout for SPI - 10ms is enough */
#define SPI_TIMEOUT             10

/* Enable SPI transmissions via DMA */
#define PCD8544_DMA_ACTIVE

/* Structure used for the GPIO definitions */
typedef struct pcd_8544_base_struct
{
    /* SPI handle and draw buffer */
    SPI_HandleTypeDef *h_spi;
    uint8_t *buffer;

    /* Port and pin pairs for the GPIOs */
    uint32_t rst_pin, ce_pin, dc_pin;
    GPIO_TypeDef *rst_port, *ce_port, *dc_port;

    /* Contrast/Bias */
    uint8_t contast, bias;

    /* Cursor position and chosen font */
    uint8_t x_pos, y_pos;

#ifdef PCD8544_DMA_ACTIVE
    /* Flag for DMA transfer status - User must not touch this field during operation !! */
    volatile bool dma_transfer;
#endif
}pcd_8544_t;

/* Initializers */
bool PCD8544_init(pcd_8544_t *init);
pcd_8544_t *PCD8544_handle_swap(pcd_8544_t *new);
bool PCD8544_refresh();

/* Utilities */
void PCD8544_fill(bool black);
bool PCD8544_invert(bool invert);
bool PCD8544_sleep_mode(bool enable);
bool PCD8544_contrast(uint8_t contrast);
bool PCD8544_bias(uint8_t bias);

/* Lines and pixels */
void PCD8544_set_pixel(uint8_t x, uint8_t y, bool color);
uint8_t PCD8544_get_pixel(uint8_t x, uint8_t y);
void PCD8544_draw_line(uint8_t x0, uint8_t x1, uint8_t y0, uint8_t y1, bool color);
void PCD8544_draw_hline(uint8_t x, uint8_t y, uint8_t len, bool color);
void PCD8544_draw_vline(uint8_t x, uint8_t y, uint8_t len, bool color);

/* Shape drawing */
void PCD8544_draw_rectangle(uint8_t x0, uint8_t x1, uint8_t y0, uint8_t y1, bool color, bool fill);
void PCD8544_draw_triangle(uint8_t x0, uint8_t x1, uint8_t x2, uint8_t y0, uint8_t y1, uint8_t y2, bool color);
void PCD8544_draw_fill_triangle(uint8_t x0, uint8_t x1, uint8_t x2, uint8_t y0, uint8_t y1, uint8_t y2, bool color);
void PCD8544_draw_circle(uint8_t x, uint8_t y, uint8_t r, bool color);
void PCD8544_draw_part_circle(uint8_t x, uint8_t y, uint8_t r, uint8_t corner, bool color);

/* Bitmaps */
void PCD8544_draw_bitmap(const uint8_t *bitmap, uint8_t x0, uint8_t y0, uint8_t len_x, uint8_t len_y);
void PCD8544_draw_bitmap_opt8(const uint8_t *bitmap, uint8_t x0, uint8_t y0, uint8_t len_x, uint8_t len_y);

/* Text */
void PCD8544_coord(uint8_t x, uint8_t p);
void PCD8544_print_str(const char *str, uint8_t option, bool invert);
void PCD8544_print_fstr(const char *str, uint8_t option, uint8_t x, uint8_t y, bool invert);




/* Todo FIX */
void PCD8544_draw_fill_circle(uint8_t x0, uint8_t y0, uint8_t r, bool color);
void PCD8544_draw_part_fill_circle(uint8_t x0, uint8_t y0, uint8_t r, uint8_t corners, int8_t delta, bool color);


#ifdef __cplusplus
}
#endif

#endif /* __PCD_8544_H */
