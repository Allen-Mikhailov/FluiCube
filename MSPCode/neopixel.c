// Neopixel docs - https://learn.adafruit.com/adafruit-neopixel-uberguide/advanced-coding

// Each Neopixel is controlled by 3 bytes. We are encoding each BIT in these bytes
// as an SPI 0b100 (for a 0) or 0b110 (for a 1). So that means that 3 Neopixel bytes (24 bits)
// become 9 SPI bytes (72 bits).

// Since the "Off" signal for a neopixel is 0x00, 0x00, 0x00, this translates to
// 0b100100100100100100 or 0x924924 924924 924924

#include <ti/devices/msp/msp.h>
#include "neopixel.h"

uint8_t pixelTxPacket[] =   {0x92, 0x4D, 0xA6, 0x92, 0x49, 0x24, 0x92, 0x4D, 0xA6};

int pixel_transmissionComplete = 0;
int pixel_idx = 0;
int pixel_repeat = 0;
int pixel_n_repeats = 8*8*5;

float pixel_buffer[8*8*6];

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

int pixel_message_len = sizeof(pixelTxPacket) / sizeof(pixelTxPacket[0]);

void convert_to_spi_byte(uint8_t byte, uint8_t *target)
{
    *target = 0x92 + ((byte & 0x80) >> 1) + ((byte & 0x40) >> 3) + ((byte & 0x20) >> 5);
    target++;
    *target = 0x49 + ((byte & 0x10) << 1) + ((byte & 0x08) >> 1);
    target++;
    *target = 0x24 + ((byte & 0x04) << 5) + ((byte & 0x02) << 3) + ((byte & 0x01) << 1);
}

void setNextLight(float r, float g, float b)
{
    uint8_t r_byte = (int) (r * 255);
    uint8_t g_byte = (int) (g * 255);
    uint8_t b_byte = (int) (b * 255);

    convert_to_spi_byte(g_byte, pixelTxPacket);
    convert_to_spi_byte(r_byte, pixelTxPacket + 3);
    convert_to_spi_byte(b_byte, pixelTxPacket + 6);
}

void update_neopixel_lights()
{
    pixel_transmissionComplete = 0;
    pixel_idx = 1;
    pixel_repeat = 0;

    NVIC_ClearPendingIRQ(SPI0_INT_IRQn);
    NVIC_EnableIRQ(SPI0_INT_IRQn);
    SPI0->TXDATA = *pixelTxPacket;

    while (!pixel_transmissionComplete)
        __WFI();
}

void SPI0_IRQHandler(void)
{
    switch (SPI0->CPU_INT.IIDX) {
        case SPI_CPU_INT_IIDX_STAT_TX_EVT: // SPI interrupt index for transmit FIFO
            SPI0->TXDATA = pixelTxPacket[pixel_idx];
            pixel_idx++;

            if (pixel_idx == pixel_message_len) {
                pixel_idx = 0;
                pixel_repeat++;
                setNextLight(0, 0, MIN(pixel_buffer[pixel_repeat] * 0.5, 1));
                if (pixel_repeat == pixel_n_repeats) {
                    pixel_transmissionComplete = 1;
                    NVIC_DisableIRQ(SPI0_INT_IRQn);
                }
            }
            break;
        default:
            break;
    }
}

//void SPI_0_INST_IRQHandler(void)
//{
//    switch (DL_SPI_getPendingInterrupt(SPI_0_INST)) {
//        case DL_SPI_IIDX_TX:
//            DL_SPI_transmitData8(SPI_0_INST, pixelTxPacket[pixel_idx]);
//            pixel_idx++;
//            setNextLight(0, 0, 0.1);
//
//            if (pixel_idx == pixel_message_len) {
//                pixel_idx = 0;
//                pixel_repeat++;
//                if (pixel_repeat == pixel_n_repeats) {
//                    pixel_transmissionComplete = 1;
//                    NVIC_DisableIRQ(SPI_0_INST_INT_IRQN);
//                }
//            }
//            break;
//        default:
//            break;
//    }
//}
