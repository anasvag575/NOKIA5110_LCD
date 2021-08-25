/*
  * Author: Anastasis Vagenas
  * Contact: anasvag29@gmail.com
  */

#include <pcd_8544.h> /* NOKIA 5110 - PCD8544 */
#include "main.h"
#include <stdio.h>
#include <pcd_8544_font.h>

/* Private variables */
SPI_HandleTypeDef hspi2;
DMA_HandleTypeDef hdma_spi2_tx;
UART_HandleTypeDef huart2;

/** the memory buffer for the LCD */
uint8_t pcd8544_buffer[LCDBUFFER_SZ] = {0xff};
pcd_8544_t pcd8544_handle =
                        {
                            .rst_pin = GPIO_PIN_0,
                            .ce_pin = GPIO_PIN_1,
                            .dc_pin = GPIO_PIN_2,

                            .rst_port = GPIOB,
                            .ce_port = GPIOB,
                            .dc_port = GPIOB,

                            .h_spi = &hspi2,
                            .buffer = pcd8544_buffer,

                            .contast = PCD8544_VOP_DEFAULT,     /* This varies wildly from screen to screen !!! */
                            .bias = PCD8544_BIAS_DEFAULT

                            #ifdef PCD8544_DMA_ACTIVE
                                , .dma_transfer = false
                            #endif
                        };

#define START_TIMER()   (DWT->CYCCNT = 0)
#define GET_TIMER()     (DWT->CYCCNT)

/* Private function prototypes */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI2_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_Core_Counter_Init(void);

/* Testing */
static void test_lcd_basic();
static void test_lcd_simple_patterns();
static void test_lcd_intermediate_patterns();
static void test_lcd_bitmaps();
static void test_lcd_text();

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* Configure the system clock */
    SystemClock_Config();

    /* Initialize all configured peripherals */
    MX_DMA_Init();
    MX_GPIO_Init();
    MX_SPI2_Init();
    MX_USART2_UART_Init();
    MX_Core_Counter_Init();

    /* Initialize PCD_8544 */
    if(!PCD8544_init(&pcd8544_handle))
    {
        printf("Initialization failed, entering infinite loop...\n");
        while(1);
    }

    /* Infinite loop */
    while (1)
    {
        printf("\n\n************BASIC TESTS************\n");
        test_lcd_basic();

        printf("\n\n************PATTERN TESTS************\n");
        test_lcd_simple_patterns();

        printf("\n\n************PATTERN TESTS (INTERMEDIATE)************\n");
        test_lcd_intermediate_patterns();

        printf("\n\n************BITMAP TESTS************\n");
        test_lcd_bitmaps();

        printf("\n\n************TEXT TESTS************\n");
        test_lcd_text();
    }
}

/* Tests basic functionalities */
static void test_lcd_basic()
{
    /* Empty the screen */
    PCD8544_fill(false);
    if(PCD8544_refresh()) printf("\t[0]Emptying screen:OK\n");
    HAL_Delay(3000);

    /* Fill the screen completely */
    PCD8544_fill(true);
    if(PCD8544_refresh()) printf("\t[1]Filling screen:OK\n");
    HAL_Delay(3000);

    /* Test inversion */
    if(PCD8544_invert(true)) printf("\t[2]Inverting screen:OK\n");
    HAL_Delay(3000);

    /* Test uninversion */
    if(PCD8544_invert(false)) printf("\t[3]Uninverting screen:OK\n");
    HAL_Delay(3000);

    /* Fill the screen using horizontal lines */
    for(uint8_t i = 0; i < LCDHEIGHT; i++) PCD8544_draw_hline(0, i, LCDWIDTH, true);
    if(PCD8544_refresh()) printf("\t[4]Filling screen with hlines:OK\n");
    HAL_Delay(3000);
    PCD8544_fill(false);

    /* Fill the screen using horizontal lines */
    for(uint8_t i = 0; i < LCDWIDTH; i++) PCD8544_draw_vline(i, 0, LCDHEIGHT, true);
    if(PCD8544_refresh()) printf("\t[5]Filling screen with vlines:OK\n");
    HAL_Delay(3000);
    PCD8544_fill(false);

    /* Fill the screen using individual sets */
    for(uint8_t i = 0; i < LCDWIDTH; i++)
        for(uint8_t j = 0; j < LCDHEIGHT; j++)
            PCD8544_set_pixel(i, j, true);

    if(PCD8544_refresh()) printf("\t[6]Filling screen with setpixel:OK\n");
    HAL_Delay(3000);
    PCD8544_fill(false);

    /* Test powering ON/OFF */
    if(PCD8544_sleep_mode(true)) printf("\t[7]Powering off display:OK\n");
    HAL_Delay(6000);

    if(PCD8544_sleep_mode(false)) printf("\t[8]Powering on display:OK\n");
    HAL_Delay(3000);
}

/* Draw basic shapes with lines/rectangles and pixel settings */
static void test_lcd_simple_patterns()
{
    bool color = true;
    for(uint8_t reps = 0; reps < 2; reps++)
    {
        /* Fill the screen with the specified color */
        PCD8544_fill(!color);

        /* Draw a chessboard */
        for(uint8_t j = 0; j < LCDHEIGHT; j++)
        {
            bool toggle = j & 0x01;
            for(uint8_t i = 0; i < LCDWIDTH; i++)
            {
                PCD8544_set_pixel(i, j, toggle);
                toggle = !toggle;
            }
        }

        if(PCD8544_refresh()) printf("\t[%s]:Chessboard pattern:OK\n", color ? "Black" : "White");
        HAL_Delay(3000);
        PCD8544_fill(!color);

        /* Draw a grid */
        for(uint8_t i = 0; i < LCDHEIGHT; i++) if((i & 0x01)) PCD8544_draw_hline(0, i, LCDWIDTH, color);
        for(uint8_t i = 0; i < LCDWIDTH; i++) if((i & 0x01)) PCD8544_draw_vline(i, 0, LCDHEIGHT, color);
        if(PCD8544_refresh()) printf("\t[%s]:Grid pattern:OK\n", color ? "Black" : "White");
        HAL_Delay(3000);
        PCD8544_fill(!color);

        /* Draw parallel horizontal lines */
        for(uint8_t i = 0; i < LCDHEIGHT; i++) if((i & 0x01)) PCD8544_draw_hline(0, i, LCDWIDTH, color);
        if(PCD8544_refresh()) printf("\t[%s]:Parallel horizontals:OK\n", color ? "Black" : "White");
        HAL_Delay(3000);
        PCD8544_fill(!color);

        /* Draw parallel vertical lines */
        for(uint8_t i = 0; i < LCDWIDTH; i++) if((i & 0x01)) PCD8544_draw_vline(i, 0, LCDHEIGHT, color);
        if(PCD8544_refresh()) printf("\t[%s]:Parallel verticals:OK\n", color ? "Black" : "White");
        HAL_Delay(3000);
        PCD8544_fill(!color);

        /* Draw non overlapping rectangles */
        for(uint8_t i = 0; i < (LCDHEIGHT - 1 - i); i+=2) PCD8544_draw_rectangle(i, LCDWIDTH - 1 - i, i, LCDHEIGHT - 1 - i, color, false);
        if(PCD8544_refresh()) printf("\t[%s]:Rectangles non-overlapping:OK\n", color ? "Black" : "White");
        HAL_Delay(3000);
        PCD8544_fill(!color);

        /* Draw vertical lines that scale should scale with height */
        for(uint8_t i = 1; i < LCDWIDTH/2; i++) PCD8544_draw_vline(i, 0, i, color);
        for(uint8_t i = LCDWIDTH/2; i < LCDWIDTH; i++) PCD8544_draw_vline(i, 0, LCDWIDTH - i, color);

        if(PCD8544_refresh()) printf("\t[%s]:Vertical line triangle:OK\n", color ? "Black" : "White");
        HAL_Delay(3000);
        PCD8544_fill(!color);

        /* Toggle foreground */
        color = !color;
    }

    /* Draw some flag patterns */
    PCD8544_fill(false);

    /* Flag pattern 1 - Cross */
    PCD8544_draw_rectangle(0, LCDWIDTH - 1, LCDHEIGHT/2 - 4, LCDHEIGHT/2 + 3, true, true);
    PCD8544_draw_rectangle(LCDWIDTH/2 - 4, LCDWIDTH/2 + 3, 0, LCDHEIGHT - 1, true, true);
    if(PCD8544_refresh()) printf("\tFlag variant 1:OK\n");
    HAL_Delay(3000);
    PCD8544_fill(false);

    /* Flag pattern 2 */
    for(uint8_t dist = 0, i = 0; i < 5; i++, dist += 10) PCD8544_draw_rectangle(0, LCDWIDTH - 1, dist + 0, dist + 4, true, true);
    PCD8544_draw_rectangle(0, 29, 0, 24, true, true);
    PCD8544_draw_rectangle(0, 29, 10, 14, false, true);
    PCD8544_draw_rectangle(13, 17, 0, 24, false, true);
    if(PCD8544_refresh()) printf("\tFlag variant 2:OK\n");
    HAL_Delay(3000);
    PCD8544_fill(false);

    /* Flag pattern 3 */
    PCD8544_draw_rectangle(0, LCDWIDTH/3 - 1, 0, LCDHEIGHT - 1, true, true);
    PCD8544_draw_rectangle(2*(LCDWIDTH/3), LCDWIDTH - 1 , 0, LCDHEIGHT - 1, true, true);
    if(PCD8544_refresh()) printf("\tFlag pattern 3:OK\n");
    HAL_Delay(3000);
    PCD8544_fill(false);
}

/* Draw intermediate 'approximate' shapes/patterns.
 * Generic lines (angle)
 * Triangles
 * Circles
 */
static void test_lcd_intermediate_patterns()
{
    /* Clear screen */
    PCD8544_fill(false);
    PCD8544_refresh();
    HAL_Delay(3000);

    /* Draw some generic lines - Should see something like a symmetric curtain */
    for(uint8_t i = 0; i < 80; i += 5) PCD8544_draw_line(0, i , 0, 70, true);
    for(uint8_t i = 0; i < 80; i += 5) PCD8544_draw_line(LCDWIDTH - 1, LCDWIDTH - 1 - i , 0, 70, true);
    if(PCD8544_refresh()) printf("\t[0]Generic line - (Curtains off):OK\n");
    HAL_Delay(3000);
    PCD8544_fill(false);

    /* Draw orthogonal triangles */
    for(uint8_t i = 0; i < 15; i += 2)
    {
        bool toggle = true;
        const uint8_t x0 = 0, x1 = 0, x2 = 5;
        const uint8_t y0 = 0, y1 = 5, y2 = 5;
        const uint8_t dist = 5;

        for(uint8_t j = 0; j < 10; j += 2)
        {
            if(toggle)
            {
                PCD8544_draw_triangle(x0 + i * dist, x1 + i * dist, x2 + i * dist,
                                      y0 + j * dist, y1 + j * dist, y2 + j * dist,
                                      toggle);
            }
            else
            {
                PCD8544_draw_fill_triangle(x0 + i * dist, x1 + i * dist, x2 + i * dist,
                                           y0 + j * dist, y1 + j * dist, y2 + j * dist,
                                           !toggle);
            }

            toggle = !toggle;
        }
    }

    /* Draw random circles */
    if(PCD8544_refresh()) printf("\t[1]Drawing triangles:OK\n");
    HAL_Delay(3000);
    PCD8544_fill(false);

    /* Draw circles - Not filled */
    PCD8544_draw_circle(0, 0, 10, true);
    PCD8544_draw_circle(0, 0, 20, true);
    PCD8544_draw_circle(0, 0, 30, true);
    PCD8544_draw_circle(40, 20, 10, true);
    PCD8544_draw_circle(40, 20, 20, true);
    PCD8544_draw_circle(40, 20, 30, true);
    if(PCD8544_refresh()) printf("\t[2]Drawing circles:OK\n");
    HAL_Delay(3000);
    PCD8544_fill(false);

    /* Draw part circles - Not filled */
    PCD8544_draw_part_circle(0, 0, 10, 0x03, true);
    PCD8544_draw_part_circle(0, 0, 20, 0x0f, true);
    PCD8544_draw_part_circle(0, 0, 30, 0x07, true);
    PCD8544_draw_part_circle(40, 20, 10, 0x02, true);
    PCD8544_draw_part_circle(40, 20, 20, 0x04, true);
    PCD8544_draw_part_circle(40, 20, 30, 0x0f, true);
    if(PCD8544_refresh()) printf("\t[3]Drawing part circles:OK\n");
    HAL_Delay(3000);
    PCD8544_fill(false);
}

/* Draw and testes bitmap functionality */
static void test_lcd_bitmaps()
{
    uint32_t time;

    /* Chessboard bitmap */
    const uint8_t bitmap1[4 * 8] = {    0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa,
                                        0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa,
                                        0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa,
                                        0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa
                                   };

    /* Elegant bitmap */
    const uint8_t epd_bitmap_rsz_pornhub_logosvg[] =
    {
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,
            0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
            0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0x0f, 0x0f, 0x0f, 0x8f, 0x8f, 0x0f, 0x0f,
            0x0f, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0x01, 0x00, 0xe0, 0xf0, 0xf0, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xf0, 0xf0, 0xe0, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x3f,
            0x3f, 0x3f, 0x1f, 0x0e, 0x00, 0x80, 0xe0, 0x7f, 0x0f, 0x07, 0xc3, 0xe3, 0xe1, 0xe3, 0x03, 0x07,
            0x1f, 0xff, 0xff, 0x03, 0x03, 0x83, 0xe3, 0xe1, 0xf3, 0xff, 0x03, 0x03, 0x03, 0xc3, 0xe3, 0xe1,
            0x83, 0x03, 0x07, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x3c, 0x3c, 0xfc, 0xfc,
            0xf8, 0xe0, 0xfc, 0xfc, 0xfc, 0xfc, 0x00, 0x00, 0xfc, 0xfc, 0xfc, 0xf8, 0xff, 0xff, 0xff, 0xfd,
            0x3c, 0x3c, 0x7c, 0xfc, 0xf8, 0xe0, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0x00, 0x00, 0x00, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xf0, 0x80, 0x00, 0x1f, 0x3f,
            0x3f, 0x3f, 0x07, 0x00, 0xc0, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
            0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
            0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x3f, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xe0, 0xff, 0xff, 0x7f, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xfe, 0xfc, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xfe, 0xfe, 0xfc, 0xfc, 0xfc, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xfe, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xfe, 0xfc, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xfe, 0xff, 0xff, 0xff, 0x80, 0x00,
            0x01, 0x03, 0x03, 0x03, 0x00, 0x00, 0x01, 0x03, 0x03, 0x03, 0x00, 0x01, 0x03, 0x03, 0x03, 0x03,
            0x01, 0x03, 0x03, 0x01, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x01, 0x00, 0x00, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe,
            0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe,
            0xfe, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };

    const uint8_t bitmap2[] =
    {
        0xFF, 0xFF, 0xDF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xDF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0xF7, 0xF7, 0xFF, 0xFF, 0xFF, 0xFF,
        0xEF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xBF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF7, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFB, 0xFF,
        0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3F, 0x1F, 0x1F, 0x5F, 0x1F,
        0x1F, 0x3F, 0xFF, 0x1F, 0x1F, 0x1F, 0xDF, 0x1F, 0x1F, 0x3F, 0xFF, 0x1F, 0x1F, 0x1F, 0x1F, 0xFF,
        0xFF, 0xFF, 0x1F, 0x1F, 0xDF, 0xDF, 0x1F, 0x1F, 0xFF, 0xFF, 0x1F, 0x1F, 0xDF, 0xDF, 0xFF, 0xEF,
        0xEF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFB, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xDF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFB, 0xFB, 0xFF, 0xFF, 0xF3,
        0xF2, 0xE2, 0xE6, 0xE4, 0xE0, 0xF1, 0xFF, 0xE0, 0xE0, 0xE0, 0xFC, 0xFC, 0xFC, 0xFE, 0xE7, 0xE0,
        0xF2, 0xF2, 0xE0, 0xE4, 0xFF, 0xFF, 0xF0, 0xE0, 0xE7, 0xE7, 0xE1, 0xF1, 0xFF, 0xFF, 0xE0, 0xE0,
        0xE6, 0xE6, 0xEF, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFD, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x01, 0x01, 0xFF, 0xFF, 0x01,
        0x01, 0xC3, 0x01, 0x01, 0xFF, 0xF9, 0x01, 0x01, 0x7F, 0x01, 0x01, 0xF9, 0x7F, 0x01, 0x21, 0x21,
        0x01, 0x0F, 0xFF, 0xFF, 0x01, 0x01, 0x7D, 0x7D, 0x01, 0x01, 0xFF, 0xFF, 0x01, 0x01, 0x6D, 0x6D,
        0xFF, 0xFF, 0x01, 0x01, 0xED, 0x01, 0x01, 0xFF, 0xFF, 0x31, 0x21, 0x65, 0x4D, 0x41, 0x11, 0x1F,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xBF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xBF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3E, 0x0E,
        0x0E, 0x1F, 0x3F, 0xFE, 0xFE, 0xFF, 0xFE, 0xFE, 0x7E, 0x1F, 0x3F, 0x3E, 0x1E, 0x1E, 0x7F, 0xFF,
        0xFE, 0xFE, 0xFF, 0x7F, 0xBE, 0x1E, 0x9F, 0x7F, 0xFE, 0xFE, 0xFE, 0xFE, 0x7E, 0x1E, 0x3F, 0x3F,
        0x1E, 0x7E, 0xFE, 0xFE, 0xFE, 0xFF, 0x3E, 0x0E, 0x0F, 0x1E, 0x3E, 0xFF, 0xFF, 0xFF, 0xFE, 0x7E,
        0x9E, 0x1E, 0xBE, 0x7F, 0xFF, 0xFF, 0xDF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xF8, 0xF8, 0xFC, 0xF8, 0xFA, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC, 0xFE, 0xF8, 0xBC, 0xFE,
        0xF8, 0xFA, 0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFD, 0xFC, 0xFC, 0xFC, 0xFC, 0xFF, 0xFF, 0xFF, 0xFC,
        0xFA, 0xF8, 0xFE, 0xFE, 0xF8, 0xFE, 0xFC, 0xFF, 0xFF, 0xFF, 0xF8, 0xF8, 0xFC, 0xF8, 0xF8, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFC, 0xFC, 0xFC, 0xFC, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xBF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    };

    START_TIMER();
    PCD8544_draw_bitmap_opt8(bitmap1, 0, 0, 8, 32);
    PCD8544_draw_bitmap(bitmap1, 17, 0, 32, 8);
    time = GET_TIMER();
    PCD8544_refresh();
    HAL_Delay(3000);
    PCD8544_fill(false);
    printf("\t[1]Drawing simple bitmap twice - Time:%ld\n", time);

    START_TIMER();
    PCD8544_draw_bitmap(epd_bitmap_rsz_pornhub_logosvg, 0, 0, 84, 48);
    time = GET_TIMER();
    PCD8544_refresh();
    HAL_Delay(3000);
    PCD8544_fill(false);
    printf("\t[2]Drawing elegant bitmap ;) - Time:%ld\n", time);

    START_TIMER();
    PCD8544_draw_bitmap_opt8(epd_bitmap_rsz_pornhub_logosvg, 0, 0, 84, 48);
    time = GET_TIMER();
    PCD8544_refresh();
    HAL_Delay(3000);
    PCD8544_fill(false);
    printf("\t[3]Drawing elegant bitmap ;) with opt routine - Time:%ld\n", time);

    START_TIMER();
    PCD8544_draw_bitmap_opt8(bitmap2, 0, 0, 84, 48);
    time = GET_TIMER();
    PCD8544_refresh();
    HAL_Delay(3000);
    PCD8544_fill(false);
    printf("\t[4]Drawing space invaders bitmap with opt routine - Time:%ld\n", time);
}

/* Draw and testes printing text functionality */
static void test_lcd_text()
{
    uint32_t time;
    uint32_t reps = 7;

    PCD8544_fill(false);

    PCD8544_coord(0, 0);
    for(uint8_t i = 0; i < reps; i++)
    {
        START_TIMER();
        PCD8544_print_str("Scroll large text!", LARGE_FONT, false);
        time = GET_TIMER();
        PCD8544_refresh();
        HAL_Delay(500);
        PCD8544_fill(false);
    }
    printf("\t[1]Printing scroll text - Time:%ld\n", time);

    PCD8544_coord(0, 0);
    for(uint8_t i = 0; i < reps; i++)
    {
        START_TIMER();
        PCD8544_print_str("Medium newline\n", MEDIUM_FONT | ALIGN_BOTTOM, false);
        time = GET_TIMER();
        PCD8544_refresh();
        HAL_Delay(500);
        PCD8544_fill(false);
    }
    printf("\t[2]Printing newline - Time:%ld\n", time);

    PCD8544_coord(0, 0);
    for(uint8_t i = 0; i < reps; i++)
    {
        START_TIMER();
        PCD8544_print_str("Inverted top centering", MEDIUM_FONT | ALIGN_UP, true);
        time = GET_TIMER();
        PCD8544_refresh();
        HAL_Delay(500);
        PCD8544_fill(false);
    }
    printf("\t[3]Printing inverted - Time:%ld\n", time);

    PCD8544_coord(0, 0);
    for(uint8_t i = 0; i < reps; i++)
    {
        START_TIMER();
        PCD8544_print_str("Small center\n", SMALL_FONT | ALIGN_CENTER, false);
        time = GET_TIMER();
        PCD8544_refresh();
        HAL_Delay(500);
        PCD8544_fill(false);
    }
    printf("\t[4]Small center - Time:%ld\n", time);

    PCD8544_coord(0, 0);
    for(uint8_t i = 0; i < reps; i++)
    {
        START_TIMER();
        PCD8544_print_str("Small top\n", SMALL_FONT | ALIGN_UP, false);
        time = GET_TIMER();
        PCD8544_refresh();
        HAL_Delay(500);
        PCD8544_fill(false);
    }
    printf("\t[5]Small top - Time:%ld\n", time);

    PCD8544_coord(0, 0);
    for(uint8_t i = 0; i < reps; i++)
    {
        START_TIMER();
        PCD8544_print_str("SMALL BOTTOM\n", SMALL_FONT | ALIGN_BOTTOM, false);
        time = GET_TIMER();
        PCD8544_refresh();
        HAL_Delay(500);
        PCD8544_fill(false);
    }
    printf("\t[6]Small bottom - Time:%ld\n", time);

    PCD8544_coord(0, 0);
    START_TIMER();
    char str [2] = "1";
    for(char i = 0x20; i != 0x7f; i++)
    {
        str[0] = i;
        PCD8544_print_str(str, SMALL_FONT | ALIGN_BOTTOM, false);
    }
    time = GET_TIMER();
    PCD8544_refresh();
    HAL_Delay(5000);
    PCD8544_fill(false);
    printf("\t[7]Printing small grammar - Time:%ld\n", time);

    PCD8544_coord(0, 0);
    START_TIMER();
    for(char i = 0x20; i != 0x7f; i++)
    {
        str[0] = i;
        PCD8544_print_str(str, MEDIUM_FONT | ALIGN_UP, false);
    }
    time = GET_TIMER();
    PCD8544_refresh();
    HAL_Delay(1000);
    PCD8544_fill(false);
    printf("\t[8]Printing medium grammar - Time:%ld\n", time);

    PCD8544_coord(0, 0);
    START_TIMER();
    for(char i = 0x20; i != 0x7f; i++)
    {
        str[0] = i;
        PCD8544_print_str(str, LARGE_FONT, false);
    }
    time = GET_TIMER();
    PCD8544_refresh();
    HAL_Delay(5000);
    PCD8544_fill(false);
    printf("\t[9]Printing large grammar - Time:%ld\n", time);

    START_TIMER();
    PCD8544_print_fstr("Hello", LARGE_FONT, 0, 0, false);
    PCD8544_print_fstr("Hello", LARGE_FONT, 4, 13, false);
    time = GET_TIMER();
    PCD8544_refresh();
    HAL_Delay(5000);
    PCD8544_fill(false);
    printf("\t[10]Printing free text - Time:%ld\n", time);

    START_TIMER();
    PCD8544_print_fstr("Hello", SMALL_FONT | ALIGN_BOTTOM, 0, 0, false);
    PCD8544_print_fstr("Hello", SMALL_FONT | ALIGN_BOTTOM, 4, 13, false);
    time = GET_TIMER();
    PCD8544_refresh();
    HAL_Delay(5000);
    PCD8544_fill(false);
    printf("\t[11]Printing free text 2 - Time:%ld\n", time);

    START_TIMER();
    PCD8544_print_fstr("Hello", MEDIUM_FONT | ALIGN_BOTTOM, 0, 0, false);
    PCD8544_print_fstr("Hello", MEDIUM_FONT | ALIGN_BOTTOM, 4, 13, false);
    PCD8544_print_fstr("Hello", MEDIUM_FONT | ALIGN_BOTTOM, 70, 20, false);
    time = GET_TIMER();
    PCD8544_refresh();

    HAL_Delay(5000);
    PCD8544_fill(false);
    printf("\t[12]Printing free text 3 - Time:%ld\n", time);
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure the main internal regulator output voltage
    */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

    /** Initializes the RCC Oscillators according to the specified parameters
    * in the RCC_OscInitTypeDef structure.
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 16;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{
    /* SPI2 parameter configuration*/
    hspi2.Instance = SPI2;
    hspi2.Init.Mode = SPI_MODE_MASTER;
    hspi2.Init.Direction = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi2.Init.NSS = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
    hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.CRCPolynomial = 10;
    if (HAL_SPI_Init(&hspi2) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 230400; //115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_14
                          |GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 PB2 PB15 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/**
  * @brief Enable DMA controller clock
  * @param None
  * @retval None
  */
static void MX_DMA_Init(void)
{
    /* DMA controller clock enable */
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* DMA interrupt init */
    /* DMA1_Stream4_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
}

static void MX_Core_Counter_Init(void)
{
  unsigned int *DWT_LAR      = (unsigned int *) 0xE0001FB0; //address of the register
  unsigned int *SCB_DEMCR    = (unsigned int *) 0xE000EDFC; //address of the register

  // ??? Helps though //
  *DWT_LAR = 0xC5ACCE55; // unlock (CM7)

  // Enable access //
  *SCB_DEMCR |= 0x01000000;

  DWT->CYCCNT = 0;      // Reset the counter //
  DWT->CTRL |= 1 ;      // Enable the counter //
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
