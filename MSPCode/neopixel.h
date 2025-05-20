#ifndef NEOPIXEL_H
#define NEOPIXEL_H

void convert_to_spi_byte(uint8_t byte, uint8_t *target);
void setNextLight(uint8_t r_byte, uint8_t g_byte, uint8_t b_byte);
void update_neopixel_lights();
void SPI_1_INST_IRQHandler(void);

extern uint8_t r_pixel_buffer[8*8*6];
extern uint8_t g_pixel_buffer[8*8*6];
extern uint8_t b_pixel_buffer[8*8*6];
#endif
