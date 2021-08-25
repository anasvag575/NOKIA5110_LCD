/*
  * Author: Anastasis Vagenas
  * Contact: anasvag29@gmail.com
  */

#include <pcd_8544.h> /* External header */
#include <pcd_8544_font.h>

#include <string.h> /* For memcpy */
#include <stdio.h>  /* TODO - For debug printf */
#include <stdlib.h>  /* TODO - For debug printf */

/* Macros to set and reset pins */
#define SET_GPIO(port, pin)     (HAL_GPIO_WritePin((port), (pin), GPIO_PIN_SET))
#define RESET_GPIO(port, pin)   (HAL_GPIO_WritePin((port), (pin), GPIO_PIN_RESET))

/* Swap macro for variables */
#define SWAP_VAR(a, b)                      \
    do                                      \
    {                                       \
        typeof((a)) __loc_var__ = (a);      \
        (a) = (b);                          \
        (b) = __loc_var__;                  \
    }while(0)

/* TODO - Remove these when done */
#define DEBUG_ACTIVE

#ifdef DEBUG_ACTIVE
    #define ASSERT_DEBUG(cond, ...) do{ if((cond)) printf(__VA_ARGS__);}while(0)
#else
    #define ASSERT_DEBUG(cond, ...)
#endif

/* Handle to be used for the screen */
static pcd_8544_t *_screen_h = NULL;

#ifdef PCD8544_DMA_ACTIVE
    /* We need to have a constant buffer for DMA transfers (commands) */
    static uint8_t command_buffer[7];

    void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
    {
        /* Chip deselect - Active low */
        if(hspi->Instance == _screen_h->h_spi->Instance)
        {
            _screen_h->dma_transfer = false;
            SET_GPIO(_screen_h->ce_port, _screen_h->ce_pin);
        }
    }
#endif

/*!
    @brief    Set a pixel's value. Internal routine, no error checking performed.
    @param    x         x-coordinate
    @param    y         y-coordinate
    @param    color     Black(True) or White(False).
*/
static void _set_single_pixel(uint8_t x, uint8_t y, bool color)
{
    uint16_t pos = ((uint16_t)y>>3) * LCDWIDTH + x;
    ASSERT_DEBUG(pos >= LCDBUFFER_SZ, "Error at _set_single_pixel %d\n", pos);

    if(color)
        _screen_h->buffer[pos] |= 1 << (y & 0x07);
    else
        _screen_h->buffer[pos] &= ~(1 << (y & 0x07));
}

/*!
    @brief    Set a pixel's value. Internal routine used for loops, no error checking performed.
    @param    pos       Position in the buffer
    @param    mask      The mask to apply
    @param    color     Black(True) or White(False).
*/
static void _set_single_pixel_opt(uint16_t pos, uint8_t mask, bool color)
{
    ASSERT_DEBUG(pos >= LCDBUFFER_SZ, "Error at _set_single_pixel_opt %d\n", pos);

    if(color)
        _screen_h->buffer[pos] |= mask;
    else
        _screen_h->buffer[pos] &= ~mask;
}

/*!
    @brief    Get a pixel's value. Internal routine, no error checking performed.
    @param    x     x-coordinate
    @param    y     y-coordinate
    @return         Black(True) or White(False).
*/
static uint8_t _get_single_pixel(uint8_t x, uint8_t y)
{
    /* First find the exact position */
    uint16_t pos = ((uint16_t)y>>3) * LCDWIDTH + x;
    ASSERT_DEBUG(pos >= LCDBUFFER_SZ, "Error at _get_single_pixel %d\n", pos);

    return (_screen_h->buffer[pos] >> (y & 0x07)) & 0x01;
}

/*!
    @brief    Get a pixel's value from a bitmap array.
    @param    bitmap    The bitmap array
    @param    x         x-coordinate
    @param    y         y-coordinate
    @param    bit_w     The bitmap width
    @return             Black(True) or White(False).
*/
static bool _get_bmp_pixel(const uint8_t *bitmap, uint8_t x, uint8_t y, uint8_t bit_w)
{
    /* First find the exact position */
    uint16_t pos = ((uint16_t)y>>3) * bit_w + x;

    return (bitmap[pos] >> (y & 0x07)) & 0x01;
}

/*!
    @brief    Get a pixel's value from a bitmap array.
    Internal routine and different variant than the normal version.
    @param    bitmap    The bitmap array
    @param    pos       The position in the array
    @param    shift     The shift value
    @return             Black(True) or White(False).
*/
static bool _get_bmp_pixel_opt(const uint8_t *bitmap, uint16_t pos, uint8_t shift)
{
    return (bitmap[pos] >> shift) & 0x01;
}

/*!
    @brief    Set multiple pixel values. Internal routine, no error checking performed.
    The routine sets pixels of a single bank(check documentation for the display's pixel layout).
    The color goes from the LSB to MSB in the buffer, or from the top to the bottom in the actual
    display.
    @param    pos     The bank position in the buffer
    @param    num     Number of pixels to color, must be less than 8(bank size).
    @param    color   Either set pixels to black(true) or white(false).
*/
static void _set_pixels_lsb2msb(uint16_t pos, uint8_t num, bool color)
{
    ASSERT_DEBUG(pos >= LCDBUFFER_SZ, "Error at _set_pixels_lsb2msb\n");
    ASSERT_DEBUG(num >= 8, "Error at _set_pixels_lsb2msb\n");

    if(color)
        _screen_h->buffer[pos] |= (1 << num) - 1;
    else
        _screen_h->buffer[pos] &= ~((1 << num) - 1);
}

/*!
    @brief    Set multiple pixel values. Internal routine, no error checking performed.
    The routine sets pixels of a single bank(check documentation for the display's pixel layout).
    The color goes from the MSB to LSB in the buffer, or from the bottom to the top in the actual
    display.
    @param    pos     The bank position in the buffer
    @param    num     Number of pixels to color, must be less than 8(bank size).
    @param    color   Either set pixels to black(true) or white(false).
*/
static void _set_pixels_msb2lsb(uint16_t pos, uint8_t num, bool color)
{
    ASSERT_DEBUG(pos >= LCDBUFFER_SZ, "Error at _set_pixels_invert_msb2lsb\n");
    ASSERT_DEBUG(num >= 8, "Error at _set_pixels_invert_msb2lsb\n");

    if(color)
        _screen_h->buffer[pos] |= ~(0xff >> num);
    else
        _screen_h->buffer[pos] &= 0xff >> num;
}

/*!
    @brief    Draws a generic line. Internal routine, it uses Bresenhm's algorithm and is based on the implementation
    by the Adafruit GFX library.
    @param    x0     Starting x-coordinate
    @param    x1     Ending x-coordinate
    @param    y0     Starting y-coordinate
    @param    y1     Ending y-coordinate
    @param    color  Black(true)/white(false)
*/
static void _draw_generic_line(uint8_t x0, uint8_t x1, uint8_t y0, uint8_t y1, bool color)
{
    int16_t steep = abs(y1 - y0) > abs(x1 - x0);

    if(steep)
    {
        SWAP_VAR(x0, y0);
        SWAP_VAR(x1, y1);
    }

    if(x0 > x1)
    {
        SWAP_VAR(x0, x1);
        SWAP_VAR(y0, y1);
    }

    int16_t dx = x1 - x0;
    int16_t dy = abs(y1 - y0);
    int16_t err = dx >> 1;
    int16_t ystep = (y0 < y1) ? 1 : -1;

    for (; x0 <= x1; x0++)
    {
        if(steep)
        {
            PCD8544_set_pixel(y0, x0, color);
        }
        else
        {
            PCD8544_set_pixel(x0, y0, color);
        }

        err -= dy;
        if(err < 0)
        {
            y0 += ystep;
            err += dx;
        }
    }
}

/*!
    @brief    SPI transmission internal routine.
    @param    data      The SPI packet buffer to be sent
    @param    nb_data   The number of packets(bytes) to be sent
    @param    type      Type of transmission, true for data else command.
    @return             Success(True) or Failure(False) of the SPI transmission.
*/
static bool _send_packet(uint8_t *data, uint16_t nb_data , bool type)
{
    HAL_StatusTypeDef ret;

    /* Data needs DC high - Command needs DC low */
    type ? SET_GPIO(_screen_h->dc_port, _screen_h->dc_pin) \
         : RESET_GPIO(_screen_h->dc_port, _screen_h->dc_pin);

    /* Chip enable - Active Low */
    RESET_GPIO(_screen_h->ce_port, _screen_h->ce_pin);

    #ifdef PCD8544_DMA_ACTIVE
        /* Set handler flag */
        _screen_h->dma_transfer = true;

        /* Transmit through SPI using DMA */
        ret = HAL_SPI_Transmit_DMA(_screen_h->h_spi, data, nb_data);
    #else
        /* Transmit through SPI */
        ret = HAL_SPI_Transmit(_screen_h->h_spi, data, nb_data, SPI_TIMEOUT);

        /* Chip disable - Active Low */
        SET_GPIO(_screen_h->ce_port, _screen_h->ce_pin);
    #endif

    return ret == HAL_OK;
}

/*!
    @brief    Creates the initialization command sequence for the screen.
    @param    command_buffer  The buffer to write the commands in
*/
static void _init_sequence(uint8_t *command_buffer)
{
    command_buffer[0] = PCD8544_FUNCTIONSET | PCD8544_EXTENDEDINSTRUCTION;     /* Enter extended instruction mode */
    command_buffer[1] = PCD8544_SETBIAS | _screen_h->bias;                     /* Set bias voltage */
    command_buffer[2] = PCD8544_SETVOP | _screen_h->contast;                   /* Set contrast */
    command_buffer[3] = PCD8544_FUNCTIONSET;                                   /* Enter normal instruction mode - Vertical display */
    command_buffer[4] = PCD8544_SETXADDR | 0;                                  /* Initialize coordinates */
    command_buffer[5] = PCD8544_SETYADDR | 0;                                  /* Initialize coordinates */
    command_buffer[6] = PCD8544_DISPLAYCONTROL | PCD8544_DISPLAYNORMAL;        /* Set display to normal */
}

/**********************************************************/
/************************ OPERATIONS **********************/
/**********************************************************/

/*!
    @brief    Initializes the display.
    @return   Success(True) or Failure(False) of the procedure.
*/
bool PCD8544_init(pcd_8544_t *init)
{
    /* Initialize the screen handle */
    _screen_h = init;

    /* Chip enable initialization - Active low */
    SET_GPIO(_screen_h->ce_port, _screen_h->ce_pin);

    /* We reset for 2ms - Active low */
    RESET_GPIO(_screen_h->rst_port, _screen_h->rst_pin);
    HAL_Delay(2);
    SET_GPIO(_screen_h->rst_port, _screen_h->rst_pin);

    /* Sanity check for contrast-bias values */
    if(_screen_h->contast > 0x7f) _screen_h->contast = 0x7f;
    if(_screen_h->bias > 0x07) _screen_h->bias = 0x07;

    /* Initialize the cursor for the text printer */
    _screen_h->x_pos = _screen_h->y_pos = 0;

    #ifndef PCD8544_DMA_ACTIVE
        /* Polling transfer - Allocate the buffer on the stack */
        uint8_t command_buffer[7];
    #else
        /* Set DMA transfer flag */
        _screen_h->dma_transfer = false;
    #endif

    /* List the base commands */
    _init_sequence(command_buffer);

    /* Send the base commands and return code */
    return _send_packet(command_buffer, 7, false);
}

/*!
    @brief    Swaps the current screen handle.
    This is used to change the current screen that the library sends commands and updates
    graphics into. Initialization for the new screen is the user's responsibility.
    @param    new  The new screen handle
    @return        The old display's handle
*/
pcd_8544_t *PCD8544_handle_swap(pcd_8544_t *new)
{
    pcd_8544_t *old = _screen_h;

    /* Update */
    _screen_h = new;

    return old;
}

/*!
    @brief    Draws the contents of the buffer on the display.
    @return   Success(True) or Failure(False) in sending the data.
*/
bool PCD8544_refresh()
{
    #ifdef PCD8544_DMA_ACTIVE
        if(_screen_h->dma_transfer) return false;
    #endif

    /* Draw and return */
    return _send_packet(_screen_h->buffer, LCDBUFFER_SZ, true);
}

/*!
    @brief    Fills the display buffer with the specified color.
    @param    color  Fill with black(true) or with white(false).
*/
void PCD8544_fill(bool color)
{
    memset(_screen_h->buffer, color ? 0xff : 0, LCDBUFFER_SZ * sizeof(uint8_t));
}

/*!
    @brief    Inverts or uninverts the display.
    @param    invert  True(Invert) and False(Uninvert).
    @return           Success(True) or Failure(False) in sending the command.
*/
bool PCD8544_invert(bool invert)
{
#ifdef PCD8544_DMA_ACTIVE
    /* DMA transfer - Make sure transfer is finished */
    if(_screen_h->dma_transfer) return false;
#else
    /* Polling transfer - Allocate the buffer on the stack */
    uint8_t command_buffer[3];
#endif

    /* Commands for inversion */
    command_buffer[0] = PCD8544_FUNCTIONSET;
    command_buffer[1] = PCD8544_DISPLAYCONTROL | (invert ? PCD8544_DISPLAYINVERTED: PCD8544_DISPLAYNORMAL);
    command_buffer[2] = PCD8544_FUNCTIONSET;

    return _send_packet(command_buffer, 3, false);
}

/*!
    @brief    Enables or disables sleep mode.
    @param    enable  Enable(true) sleep mode or disable(false).
    @return           Success(True) or Failure(False) in sending the command.
*/
bool PCD8544_sleep_mode(bool enable)
{
    #ifdef PCD8544_DMA_ACTIVE
        /* DMA transfer - Make sure transfer is finished */
        if(_screen_h->dma_transfer) return false;
    #else
        /* Polling transfer - Allocate the buffer on the stack */
        uint8_t command_buffer[7];
    #endif

    if(enable) /* Buffer and settings are saved */
    {
        command_buffer[1] = PCD8544_FUNCTIONSET | PCD8544_POWERDOWN;
        return _send_packet(command_buffer, 1, false);
    }

    /* Repeat basic initialization */
    _init_sequence(command_buffer);

    return _send_packet(command_buffer, 7, false);
}

/*!
    @brief    Set display's contrast value.
    @param    contrast  The contrast value.
    @return             Success(True) or Failure(False) in sending the command.
*/
bool PCD8544_contrast(uint8_t contrast)
{
    #ifdef PCD8544_DMA_ACTIVE
        /* DMA transfer - Make sure transfer is finished */
        if(_screen_h->dma_transfer) return false;
    #else
        /* Polling transfer - Allocate the buffer on the stack */
        uint8_t command_buffer[3];
    #endif

    /* Update contrast value */
    _screen_h->contast = (contrast < 0x7f) ? contrast : 0x7f;

    command_buffer[0] = PCD8544_FUNCTIONSET | PCD8544_EXTENDEDINSTRUCTION;
    command_buffer[1] = PCD8544_SETVOP | _screen_h->contast;
    command_buffer[2] = PCD8544_FUNCTIONSET;

    return _send_packet(command_buffer, 3, false);
}

/*!
    @brief    Set display's bias value.
    @param    bias  The bias value.
    @return         Success(True) or Failure(False) in sending the command.
*/
bool PCD8544_bias(uint8_t bias)
{
    #ifdef PCD8544_DMA_ACTIVE
        /* DMA transfer - Make sure transfer is finished */
        if(_screen_h->dma_transfer) return false;
    #else
        /* Polling transfer - Allocate the buffer on the stack */
        uint8_t command_buffer[3];
    #endif

    /* Update bias value */
    _screen_h->bias = (bias < 0x07) ? bias : 0x07;

    command_buffer[0] = PCD8544_FUNCTIONSET | PCD8544_EXTENDEDINSTRUCTION;
    command_buffer[1] = PCD8544_SETBIAS | _screen_h->bias;
    command_buffer[2] = PCD8544_FUNCTIONSET;

    return _send_packet(command_buffer, 3, false);
}

/*!
    @brief    Return SPI's transfer state.
    @return   True in case a SPI transfer is active, otherwise false (polling mode always false).
*/
bool PCD8544_transfer()
{

}

/**********************************************************/
/************************ GRAPHICS ************************/
/**********************************************************/

/*!
    @brief    Set a pixel's value.
    @param    x       x-coordinate
    @param    y       y-coordinate
    @param    color   Black(true) or white(false)
*/
void PCD8544_set_pixel(uint8_t x, uint8_t y, bool color)
{
    /* Sanity check */
    if((x >= LCDWIDTH) || (y >= LCDHEIGHT)) return;

    /* Call the internal routine */
    _set_single_pixel(x, y, color);
}

/*!
    @brief    Returns a pixel's value.
    @param    x   x-coordinate
    @param    y   y-coordinate
    @return       In case of error, 0xFF is returned, otherwise true or false.
*/
uint8_t PCD8544_get_pixel(uint8_t x, uint8_t y)
{
    /* Return max value in case of failure */
    return ((x >= LCDWIDTH) || (y >= LCDHEIGHT)) ? 0xff : _get_single_pixel(x, y);
}

/*!
    @brief    Draw a horizontal line.
    @param    x      Left-most x-coordinate
    @param    y      Left-most y-coordinate
    @param    len    The length of the line including the starting pixel
    @param    color  Black(true)/white(false)
*/
void PCD8544_draw_hline(uint8_t x, uint8_t y, uint8_t len, bool color)
{
    /* Sanity check - x value is taken care of by the loop conditions */
    if(y >= LCDHEIGHT || x >= LCDWIDTH) return;

    uint16_t pos = (y >> 3) * LCDWIDTH + x;
    if(((uint16_t)x + len) > LCDWIDTH) len = LCDWIDTH - x;
    uint8_t common_mask = 1 << (y & 0x07);

    if(color)
    {
        for(uint8_t i = 0; i < len; i++)
        {
            ASSERT_DEBUG((pos + i) >= LCDBUFFER_SZ, "Error at PCD8544_draw_hline\n");
            _screen_h->buffer[pos + i] |= common_mask;
        }
    }
    else
    {
        for(uint8_t i = 0; i < len; i++)
        {
            ASSERT_DEBUG((pos + i) >= LCDBUFFER_SZ, "Error at PCD8544_draw_hline\n");
            _screen_h->buffer[pos + i] &= ~common_mask;
        }
    }
}

/*!
    @brief    Draw a vertical line.
    @param    x      Left-most x-coordinate
    @param    y      Left-most y-coordinate
    @param    len    The length of the line including the starting pixel
    @param    color  Black(true)/white(false)
*/
void PCD8544_draw_vline(uint8_t x, uint8_t y, uint8_t len, bool color)
{
    /* Sanity check - y value is taken care of by the loop conditions */
    if(x >= LCDWIDTH || y >= LCDHEIGHT) return;

    if(((uint16_t)y + len) >= LCDHEIGHT) len = LCDHEIGHT - y;

    uint8_t byte_in = color ? 0xff : 0;
    uint16_t pos = (y >> 3) * LCDWIDTH + x;
    uint8_t temp = y & 0x07;

    /* Partial bank fill */
    if(temp)
    {
        ASSERT_DEBUG(pos >= LCDBUFFER_SZ, "Error at PCD8544_draw_vline\n");
        uint8_t pixel_num = 8 - temp;

        if(len <= pixel_num) /* Sub-case that needs to be handled */
        {
            pixel_num = len;
            _set_pixels_msb2lsb(pos, pixel_num, color);
            return;
        }

        _set_pixels_msb2lsb(pos, pixel_num, color);
        pos += LCDWIDTH;
        len -= pixel_num;
    }

    /* Number of complete banks to fill */
    while(len >= 8)
    {
        ASSERT_DEBUG(pos >= LCDBUFFER_SZ, "Error at PCD8544_draw_vline\n");
        _screen_h->buffer[pos] = byte_in;
        pos += LCDWIDTH;
        len -= 8;
    }

    /* Draw leftovers */
    if(len) _set_pixels_lsb2msb(pos, len, color);
}

/*!
    @brief    Draw a generic line.
    @param    x0     Starting x-coordinate
    @param    x1     Ending x-coordinate
    @param    y0     Starting y-coordinate
    @param    y1     Ending y-coordinate
    @param    color  Black(true)/white(false)
*/
void PCD8544_draw_line(uint8_t x0, uint8_t x1, uint8_t y0, uint8_t y1, bool color)
{
    if(x0 == x1) /* Horizontal line -> Call optimized version */
    {
        if(y0 > y1) SWAP_VAR(y0, y1);

        PCD8544_draw_vline(x0, y0, y1 - y0 + 1, color);
    }
    else if(y0 == y1) /* Vertical line -> Call optimized version */
    {
        if(x0 > x1) SWAP_VAR(x0, x1);

        PCD8544_draw_hline(x0, y0, x1 - x0 + 1, color);
    }
    else /* General case */
    {
        _draw_generic_line(x0, x1, y0, y1, color);
    }
}

/*!
    @brief    Draw a rectangle.
    @param    x0     Upper left x-coordinate
    @param    x1     Lower right x-coordinate
    @param    y0     Upper left y-coordinate
    @param    y1     Lower right y-coordinate
    @param    color  Black(true)/white(false)
    @param    fill   If true also fill the rectangle with the specified color
*/
void PCD8544_draw_rectangle(uint8_t x0, uint8_t x1, uint8_t y0, uint8_t y1, bool color, bool fill)
{
    /* Sanity check */
    if(x0 >= LCDWIDTH || y0 >= LCDHEIGHT) return;

    /* Just in case mistakes were made */
    if(x0 > x1) SWAP_VAR(x0, x1);
    if(y0 > y1) SWAP_VAR(y0, y1);

    /* Set the line lengths */
    uint8_t len_x = x1 - x0 + 1;
    uint8_t len_y = y1 - y0 + 1;

    if(!fill)
    {
        /* Connect 4 lines together */
        PCD8544_draw_hline(x0, y0, len_x, color);
        PCD8544_draw_hline(x0, y1, len_x, color);
        PCD8544_draw_vline(x0, y0, len_y, color);
        PCD8544_draw_vline(x1, y0, len_y, color);
        return;
    }

//    /* It's more efficient to use vertical lines to draw for filling.
//     * That's the case since fewer memory accesses and instructions happen (on average) */
//    for(uint8_t i = 0; i < len_x; i++) PCD8544_draw_vline(x0 + i, y0, len_y, color);

    if(((uint16_t)y0 + len_y) >= LCDHEIGHT) len_y = LCDHEIGHT - y0;
    if(((uint16_t)x0 + len_x) >= LCDWIDTH) len_x = LCDWIDTH - x0;

    uint8_t byte_in = color ? 0xff : 0;
    uint16_t pos = (y0 >> 3) * LCDWIDTH + x0;
    uint8_t temp = y0 & 0x07;

    /* Partial bank fill */
    if(temp)
    {
        ASSERT_DEBUG(pos >= LCDBUFFER_SZ, "Error at PCD8544_draw_vline\n");
        uint8_t pixel_num = 8 - temp;

        if(len_y <= pixel_num) /* Sub-case that needs to be handled */
        {
            pixel_num = len_y;
            for(uint8_t i = 0; i < len_x; i++) _set_pixels_msb2lsb(pos + i, pixel_num, color);
            return;
        }

        for(uint8_t i = 0; i < len_x; i++) _set_pixels_msb2lsb(pos + i, pixel_num, color);
        pos += LCDWIDTH;
        len_y -= pixel_num;
    }

    /* Number of complete banks to fill */
    while(len_y >= 8)
    {
        ASSERT_DEBUG(pos >= LCDBUFFER_SZ, "Error at PCD8544_draw_vline\n");
        memset(_screen_h->buffer + pos, byte_in, len_x * sizeof(uint8_t));
        pos += LCDWIDTH;
        len_y -= 8;
    }

    /* Draw leftovers */
    if(len_y)
    {
        for(uint8_t i = 0; i < len_x; i++) _set_pixels_lsb2msb(pos + i, len_y, color);
    }
}

/*!
    @brief    Draws a triangle. Also taken by the Adafruit GFX library.
    @param    x0     First x-coordinate
    @param    x1     Second x-coordinate
    @param    x2     Third x-coordinate
    @param    y0     First y-coordinate
    @param    y1     Second y-coordinate
    @param    y2     Third y-coordinate
    @param    color  Black(true)/white(false)
*/
void PCD8544_draw_triangle(uint8_t x0, uint8_t x1, uint8_t x2, uint8_t y0, uint8_t y1, uint8_t y2, bool color)
{
    PCD8544_draw_line(x0, x1, y0, y1, color);
    PCD8544_draw_line(x1, x2, y1, y2, color);
    PCD8544_draw_line(x0, x2, y0, y2, color);
}

/*!
    @brief    Draws a filled triangle. Also taken by the Adafruit GFX library.
    @param    x0     First x-coordinate
    @param    x1     Second x-coordinate
    @param    x2     Third x-coordinate
    @param    y0     First y-coordinate
    @param    y1     Second y-coordinate
    @param    y2     Third y-coordinate
    @param    color  Triangle color, Black(true)/white(false)
*/
void PCD8544_draw_fill_triangle(uint8_t x0, uint8_t x1, uint8_t x2, uint8_t y0, uint8_t y1, uint8_t y2, bool color)
{
    uint8_t a, b, y, last;

    /* Sort coordinates by Y order (y2 >= y1 >= y0) */
    if (y0 > y1)
    {
        SWAP_VAR(y0, y1);
        SWAP_VAR(x0, x1);
    }

    if (y1 > y2)
    {
        SWAP_VAR(y2, y1);
        SWAP_VAR(x2, x1);
    }

    if (y0 > y1)
    {
        SWAP_VAR(y0, y1);
        SWAP_VAR(x0, x1);
    }

    /* Handle awkward all-on-same-line case as its own thing */
    if(y0 == y2)
    {
        a = b = x0;
        if (x1 < a)       a = x1;
        else if (x1 > b)  b = x1;

        if (x2 < a)       a = x2;
        else if (x2 > b)  b = x2;

        PCD8544_draw_hline(a, y0, b - a + 1, color);
        return;
    }

    uint8_t dx01 = x1 - x0, dy01 = y1 - y0, dx02 = x2 - x0, dy02 = y2 - y0,
            dx12 = x2 - x1, dy12 = y2 - y1;
    uint16_t sa = 0, sb = 0;

    /* For upper part of triangle, find scanline crossings for segments 0-1 and 0-2.
     * If y1=y2 (flat-bottomed triangle), the scanline y1
     * is included here (and second loop will be skipped, avoiding a /0
     * error there), otherwise scanline y1 is skipped here and handled
     * in the second loop...which also avoids a /0 error here if y0=y1
     * (flat-topped triangle).
     */

    /* Include y1 scanline(?) or skip it(:) */
    last = (y1 == y2) ? y1 : (y1 - 1);

    for (y = y0; y <= last; y++)
    {
        a = x0 + sa / dy01;
        b = x0 + sb / dy02;
        sa += dx01;
        sb += dx02;
        /* longhand:
        a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
        b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
        */
        if (a > b) SWAP_VAR(a, b);
        PCD8544_draw_hline(a, y, b - a + 1, color);
    }

    /* For lower part of triangle, find scanline crossings for segments
     * 0-2 and 1-2.  This loop is skipped if y1=y2.
     */
    sa = (uint16_t)dx12 * (y - y1);
    sb = (uint16_t)dx02 * (y - y0);
    for (; y <= y2; y++)
    {
        a = x1 + sa / dy12;
        b = x0 + sb / dy02;
        sa += dx12;
        sb += dx02;
        /* longhand:
        a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
        b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
        */
        if (a > b) SWAP_VAR(a, b);
        PCD8544_draw_hline(a, y, b - a + 1, color);
    }
}

/*!
    @brief    Draws a circle - Uses the Midpoint circle algorithm.
    @param    x0   Center x-coordinate
    @param    y0   Center y-coordinate
    @param    r    Circle radius
    @param    color - black(true)/white(false)
*/
void PCD8544_draw_circle(uint8_t x, uint8_t y, uint8_t r, bool color)
{
    int8_t a = 0;
    int8_t b = r;
    int8_t p = 1 - r;

    do
    {
        PCD8544_set_pixel(x+a, y+b, color);
        PCD8544_set_pixel(x-a, y+b, color);
        PCD8544_set_pixel(x+a, y-b, color);
        PCD8544_set_pixel(x-a, y-b, color);
        PCD8544_set_pixel(x+b, y+a, color);
        PCD8544_set_pixel(x-b, y+a, color);
        PCD8544_set_pixel(x+b, y-a, color);
        PCD8544_set_pixel(x-b, y-a, color);

        if(p < 0)
        {
            p += (3 + 2*a);
            a++;
        }
        else
        {
            p += (5 + 2*(a-b));
            a++;
            b--;
        }
    }while(a <=b);
}

/*!
    @brief    Draws a circle part - Uses the Midpoint circle algorithm.
    @param    x0        Center x-coordinate
    @param    y0        Center y-coordinate
    @param    r         Circle radius
    @param    corner    Which corners to draw, using coordinates
    (0x04 - SE, 0x2 - NE, 0x08 - SW, 0x01 - NW)
    @param    color     Black(true)/White(false)
*/
void PCD8544_draw_part_circle(uint8_t x, uint8_t y, uint8_t r, uint8_t corner, bool color)
{
    int8_t a = 0;
    int8_t b = r;
    int8_t p = 1 - r;

    do
    {
        if(corner & 0x4)
        {
            PCD8544_set_pixel(x+a, y+b, color);
            PCD8544_set_pixel(x+b, y+a, color);
        }

        if(corner & 0x2)
        {
            PCD8544_set_pixel(x+a, y-b, color);
            PCD8544_set_pixel(x+b, y-a, color);
        }

        if(corner & 0x8)
        {
            PCD8544_set_pixel(x-a, y+b, color);
            PCD8544_set_pixel(x-b, y+a, color);
        }

        if(corner & 0x1)
        {
            PCD8544_set_pixel(x-a, y-b, color);
            PCD8544_set_pixel(x-b, y-a, color);
        }

        if(p < 0)
        {
            p += (3 + 2*a);
            a++;
        }
        else
        {
            p += (5 + 2*(a-b));
            a++;
            b--;
        }
    }while(a <=b);
}

/**********************************************************/
/************************ BITMAPS *************************/
/**********************************************************/

/*!
    @brief    Draws a bitmap on the screen.
    @param    bitmap    The bitmap array
    @param    x0        Leftmost x-coordinate
    @param    y0        Leftmost y-coordinate
    @param    len_x     The width of the bitmap
    @param    len_y     The height of the bitmap
*/
void PCD8544_draw_bitmap(const uint8_t *bitmap, uint8_t x0, uint8_t y0, uint8_t len_x, uint8_t len_y)
{
    /* Illegal format of the bitmap or initial position */
    if(x0 >= LCDWIDTH || y0 >= LCDHEIGHT) return;

    /* Fix drawing lengths */
    uint8_t draw_ylen = (((uint16_t)y0 + len_y) >= LCDHEIGHT) ? (LCDHEIGHT - y0) : len_y;
    uint8_t draw_xlen = (((uint16_t)y0 + len_y) >= LCDWIDTH) ? (LCDWIDTH - x0) : len_x;

    for(uint8_t j = 0; j < draw_ylen; j++)
    {
        /* Since we use the opt version of set, we calculate it ourselves */
        uint8_t mask = 1 << ((y0 + j) & 0x07);
        uint8_t bmp_shift = (j & 0x07);
        uint16_t pos = ((y0 + j) >> 3) * LCDWIDTH + x0;
        uint16_t pos_src = (j >> 3) * len_x;

        for(uint8_t i = 0; i < draw_xlen; i++)
        {
            bool bmp_color = _get_bmp_pixel_opt(bitmap, pos_src + i, bmp_shift);
            _set_single_pixel_opt(pos + i, mask, bmp_color);
        }
    }
}

/*!
    @brief    Draws a bitmap on the screen.
    Optimized version that draws bitmaps with height a multiple of 8
    and that start from a multiple of 8 y-coordinate.
    Basically we draw from the start of a bank continuously.

    @param    bitmap    The bitmap array
    @param    x0        Leftmost x-coordinate
    @param    y0        Leftmost y-coordinate
    @param    len_x     The width of the bitmap
    @param    len_y     The height of the bitmap
*/
void PCD8544_draw_bitmap_opt8(const uint8_t *bitmap, uint8_t x0, uint8_t y0, uint8_t len_x, uint8_t len_y)
{
    /* Illegal format of the bitmap or initial position or height */
    if(x0 >= LCDWIDTH || y0 >= LCDHEIGHT) return;
    if((y0 & 0x07) || (len_y & 0x07)) return;

    /* Fix drawing length */
    if(((uint16_t)y0 + len_y) >= LCDHEIGHT) len_y = LCDHEIGHT - y0;
    if(((uint16_t)x0 + len_x) >= LCDWIDTH) len_x = LCDWIDTH - x0;

    uint8_t full_banks = len_y >> 3;
    uint16_t pos = (y0 >>3) * LCDWIDTH + x0;
    uint16_t pos_src = 0;

    for(uint8_t j = 0; j < full_banks; j++)
    {
        ASSERT_DEBUG((pos) >= LCDBUFFER_SZ, "Error at PCD8544_draw_bitmap_opt8 -> %d\n", pos);
        ASSERT_DEBUG((pos_src) >= (len_x*len_y), "Error at PCD8544_draw_bitmap_opt8 -> %d\n", pos_src);

        memcpy(_screen_h->buffer + pos, bitmap + pos_src, len_x * sizeof(uint8_t));
        pos += LCDWIDTH;
        pos_src += len_x;
    }
}

/**********************************************************/
/************************* TEXT ***************************/
/**********************************************************/

/*!
    @brief    Set the cursor position for the default printer.
    @param    x  x-coordinate
    @param    y  y-coordinate
*/
void PCD8544_coord(uint8_t x, uint8_t y)
{
    /* X-coordinate is common for both fonts */
    if(x < LCDWIDTH) _screen_h->x_pos = x;
    if(y < LCDHEIGHT) _screen_h->y_pos = y >> 3;
}

/*!
    @brief    Draws a string on the string.
    This is the default printer that draws on the preassigned coordinates with the
    preassigned font (small, medium, large).
    The printing is done on the bank borders, so text can be aligned UP, CENTER and LOW.

    CENTER:    Text aligned exactly at the center
    BOTTOM:    Text aligned exactly at the lowest part
    TOP:       Text aligned exactly at the highest possible spot

    In the case of LARGE text, no option is available.
    In the case of MEDIUM text, only TOP and BOTTOM options available.
    In the case of SMALL text, all options are available.

    @param    str       The string to print
    @param    option    The options (font and potential centering)
    @param    invert    Flag to invert the text, if true inverts (black bg with white character)
    otherwise left as is
*/
void PCD8544_print_str(const char *str, uint8_t option, bool invert)
{
    /* Sanity check */
    if(!str) return;

    uint8_t width, shift, byte_num, *font;
    const char offset = 0x20; /* For now this is constant - TODO No big number fonts */

    /* Get the parameters of the text */
    switch(option & FONT_MASK)
    {
        case LARGE_FONT:
        {
            shift = 0;
            width = 6;
            byte_num = 6;
            font = (uint8_t *)large_font; // For Compiler warnings //
            break;
        }
        case MEDIUM_FONT:
        {
            shift = (option & ALIGMENT_MASK) >> 1; /* Only up or bottom alignment for medium */
            width = 5;
            byte_num = 5;
            font = (uint8_t *)medium_font; // For Compiler warnings //
            break;
        }
        case SMALL_FONT:
        {
            shift = option & ALIGMENT_MASK;
            width = 4;
            byte_num = 3;
            font = (uint8_t *)small_font; // For Compiler warnings //
            break;
        }
        default: return; /* Illegal option */
    }

    /* Print buffer in case we need to edit a character */
    uint8_t buffer[6];

    for(; *str; str++)
    {
        /* Screen bounds exceeded or newline found */
        if((_screen_h->x_pos + width) >= LCDWIDTH || *str == '\n')
        {
            _screen_h->x_pos = 0;
            _screen_h->y_pos++;
        }

        /* Screen bounds exceeded, reset back to start */
        if(_screen_h->y_pos >= LCDHEIGHT/8) _screen_h->y_pos = 0;

        if(*str >= offset)
        {
            uint16_t dest_pos = (uint16_t)_screen_h->y_pos * LCDWIDTH + _screen_h->x_pos;
            uint16_t src_pos = (*str - offset) * byte_num;

            /* Copy to the print buffer */
            memcpy(buffer, font + src_pos, byte_num * sizeof(uint8_t));

            /* Small font has to be decoded since the bytes are packed */
            if((option & FONT_MASK) == SMALL_FONT)
            {
                /* Unpack the 3 bytes into 4 */
                buffer[3] = (buffer[0] & 0xfc) >> 2;
                buffer[4] = ((buffer[0] & 0x03) << 4) | ((buffer[1] & 0xf0) >> 4);
                buffer[5] = ((buffer[1] & 0x0f) << 2) | ((buffer[2] & 0xc0) >> 6);
                uint8_t temp = buffer[2] & 0x3f;

                /* Correct position */
                buffer[0] = buffer[3];
                buffer[1] = buffer[4];
                buffer[2] = buffer[5];
                buffer[3] = temp;
            }

            /* Shifting */
            if(shift)
            {
                for(uint8_t i = 0; i < width; i++) buffer[i] = buffer[i] << shift;
            }

            /* Invert option */
            if(invert)
            {
                for(uint8_t i = 0; i < width; i++) buffer[i] = ~buffer[i];
            }

            memcpy(_screen_h->buffer + dest_pos, buffer, width * sizeof(uint8_t));

            _screen_h->x_pos += width;
        }
    }
}

/*!
    @brief    Draws a string on the string.
    This variant prints a string on any xy coordinate in the screen (starting positions) freely, hence
    the 'f' in method name. The starting position is the uppermost left point of where a character should be.
    @param    str       The string to print
    @param    option    Font type (Alignment is not needed here)
    @param    x         Starting x-coordinate
    @param    y         Starting y-coordinate
    @param    invert    Flag to invert the text, if true inverts (black bg with white character)
    otherwise left as is.
*/
void PCD8544_print_fstr(const char *str, uint8_t option, uint8_t x, uint8_t y, bool invert)
{
    /* Sanity check */
    if(!str) return;

    uint8_t width, height, byte_num, *font;
    const char offset = 0x20; /* For now this is constant - TODO No big number fonts */

    /* Get the parameters of the text */
    switch(option & FONT_MASK)
    {
        case LARGE_FONT:
        {
            height = 8;
            width = 6;
            byte_num = 6;
            font = (uint8_t *)large_font; // For Compiler warnings //
            break;
        }
        case MEDIUM_FONT:
        {
            height = 7;
            width = 5;
            byte_num = 5;
            font = (uint8_t *)medium_font; // For Compiler warnings //
            break;
        }
        case SMALL_FONT:
        {
            height = 6;
            width = 4;
            byte_num = 3;
            font = (uint8_t *)small_font; // For Compiler warnings //
            break;
        }
        default: return; /* Illegal option */
    }

    /* Print buffer in case we need to edit a character */
    uint8_t buffer[6];

    for(; *str; str++)
    {
        /* Screen bounds exceeded or newline found */
        if((x + width) >= LCDWIDTH || *str == '\n')
        {
            x = 0;
            y += height;
        }

        /* Screen bounds exceeded, reset back to start */
        if(y >= LCDHEIGHT) y = 0;

        if(*str >= offset)
        {
            uint16_t src_pos = (*str - offset) * byte_num;

            /* Copy to the print buffer */
            memcpy(buffer, font + src_pos, byte_num * sizeof(uint8_t));

            /* Small font has to be decoded since the bytes are packed */
            if((option & FONT_MASK) == SMALL_FONT)
            {
                /* Unpack the 3 bytes into 4 */
                buffer[3] = (buffer[0] & 0xfc) >> 2;
                buffer[4] = ((buffer[0] & 0x03) << 4) | ((buffer[1] & 0xf0) >> 4);
                buffer[5] = ((buffer[1] & 0x0f) << 2) | ((buffer[2] & 0xc0) >> 6);
                uint8_t temp = buffer[2] & 0x3f;

                /* Correct position */
                buffer[0] = buffer[3];
                buffer[1] = buffer[4];
                buffer[2] = buffer[5];
                buffer[3] = temp;
            }

            /* Invert option */
            if(invert)
            {
                for(uint8_t i = 0; i < width; i++) buffer[i] = ~buffer[i];
            }

            /* Draw the bitmap */
            PCD8544_draw_bitmap(buffer, x, y, width, height * sizeof(uint8_t));

            x += width;
        }
    }
}


/* TODO - Fix these they do not work correctly */

/*!
    @brief    Draws a circle - Uses the Midpoint circle alogrithm, also taken from Adafruit GFX library.
    @param    x0   Center x-coordinate
    @param    y0   Center y-coordinate
    @param    r    Circle radius
    @param    color - black(true)/white(false)
*/
void PCD8544_draw_circle_tmp(uint8_t x0, uint8_t y0, uint8_t r, bool color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    /* Write out the cross */
    PCD8544_set_pixel(x0, y0 + r, color);
    PCD8544_set_pixel(x0, y0 - r, color);
    PCD8544_set_pixel(x0 + r, y0, color);
    PCD8544_set_pixel(x0 - r, y0, color);

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }

        x++;
        ddF_x += 2;
        f += ddF_x;

        PCD8544_set_pixel(x0 + x, y0 + y, color);
        PCD8544_set_pixel(x0 - x, y0 + y, color);
        PCD8544_set_pixel(x0 + x, y0 - y, color);
        PCD8544_set_pixel(x0 - x, y0 - y, color);
        PCD8544_set_pixel(x0 + y, y0 + x, color);
        PCD8544_set_pixel(x0 - y, y0 + x, color);
        PCD8544_set_pixel(x0 + y, y0 - x, color);
        PCD8544_set_pixel(x0 - y, y0 - x, color);
    }
}

void PCD8544_draw_fill_circle(uint8_t x0, uint8_t y0, uint8_t r, bool color)
{
    PCD8544_draw_vline(x0, y0 - r, 2 * r + 1, color);
    PCD8544_draw_part_fill_circle(x0, y0, r, 3, 0, color);
}

/* Draws a quarter up to a full circle - Uses the Midpoint circle alogrithm, also taken from Adafruit GFX library. */
void PCD8544_draw_part_circle_tmp(uint8_t x0, uint8_t y0, uint8_t r, uint8_t cornername, bool color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }

        x++;
        ddF_x += 2;
        f += ddF_x;

        if (cornername & 0x4)
        {
            PCD8544_set_pixel(x0 + x, y0 + y, color);
            PCD8544_set_pixel(x0 + y, y0 + x, color);
        }

        if (cornername & 0x2)
        {
            PCD8544_set_pixel(x0 + x, y0 - y, color);
            PCD8544_set_pixel(x0 + y, y0 - x, color);
        }

        if (cornername & 0x8)
        {
            PCD8544_set_pixel(x0 - y, y0 + x, color);
            PCD8544_set_pixel(x0 - x, y0 + y, color);
        }

        if (cornername & 0x1)
        {
            PCD8544_set_pixel(x0 - y, y0 - x, color);
            PCD8544_set_pixel(x0 - x, y0 - y, color);
        }
    }
}

void PCD8544_draw_part_fill_circle(uint8_t x0, uint8_t y0, uint8_t r, uint8_t corners, int8_t delta, bool color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;
    int16_t px = x;
    int16_t py = y;

    delta++; // Avoid some +1's in the loop

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }

        x++;
        ddF_x += 2;
        f += ddF_x;

        // These checks avoid double-drawing certain lines, important
        // for the SSD1306 library which has an INVERT drawing mode.
        if (x < (y + 1))
        {
            if (corners & 1) PCD8544_draw_vline(x0 + x, y0 - y, 2 * y + delta, color);
            if (corners & 2) PCD8544_draw_vline(x0 - x, y0 - y, 2 * y + delta, color);
        }

        if (y != py)
        {
            if (corners & 1) PCD8544_draw_vline(x0 + py, y0 - px, 2 * px + delta, color);
            if (corners & 2) PCD8544_draw_vline(x0 - py, y0 - px, 2 * px + delta, color);

            py = y;
        }

        px = x;
    }
}

