# STM32 NOKIA5110 LCD Library

A library for the NOKIA5110 LCD (PCD8544) display for STM32 devices.

### Description

This library is implemented using the HAL API, in order to have portability and ease of use across all STM32 MCUs. Most of the library is based on other implementations from these repositories (check them out):

- https://github.com/adafruit/Adafruit-GFX-Library (Graphics library for Arduino)
- https://github.com/eziya/STM32F4_HAL_EXAMPLES/tree/master/STM32F4_HAL_SPI_DMA_NOKIA5110 (Alternative DMA Nokia5110 library)
- http://www.rinkydinkelectronics.com/library.php?id=44 (Another well documented Arduino library)

### Supported operations

The library supports display configuration, graphics and text drawing. Specifically:

- Both SPI polling and DMA transmissions are supported
- All display settings/commands are supported (i.e., contrast, bias)
- Graphics and basic shapes (i.e., rectangles, circles)
- Text printing with multiple fonts

### In progress

- Filled circles, round rectangles (fill/no fill)
- Doxygen documentation
- Code formatting

Feel free to inform me, in case any issue is found.

