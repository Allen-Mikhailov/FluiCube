#ifndef NEOPIXEL_H
#define NEOPIXEL_H

void convert_to_spi_byte(uint8_t byte, uint8_t *target);
void setNextLight(float r, float g, float b);
void update_neopixel_lights();
void SPI_1_INST_IRQHandler(void);

extern float pixel_buffer[8*8*6];
#endif
