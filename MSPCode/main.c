#include "simon_helper.h"
#include "neopixel.h"
#include "simulation.h"
#include <math.h>

/* Maximum buffer size defined for this example */
#define MAX_BUFFER_SIZE (20)

/* Dummy data sent when receiving data from SPI Peripheral */
#define DUMMY_DATA (0x00)

/* State machine to keep track of the current SPI Controller mode */
typedef enum SPI_ControllerModeEnum {
    IDLE_MODE,
    TX_REG_ADDRESS_WRITE_MODE,
    TX_REG_ADDRESS_READ_MODE,
    WRITE_DATA_MODE,
    READ_DATA_MODE,
    TIMEOUT_MODE,
    WRITE_COMPLETE_MODE,
} SPI_Controller_Mode;

/* Buffer used to receive data in the ISR */
uint8_t gRxBuffer[MAX_BUFFER_SIZE] = {0};
/* Buffer used to transmit data in the ISR */
uint8_t gTxBuffer[MAX_BUFFER_SIZE] = {0};

/* Used to track the state of the software state machine */
volatile SPI_Controller_Mode gControllerMode = IDLE_MODE;
/* Number of bytes left to receive */
volatile uint8_t gRxByteCount = 0;
/* Index of the next byte to be received in gRxBuffer */
volatile uint8_t gRxIndex = 0;
/* Number of bytes left to transfer */
volatile uint8_t gTxByteCount = 0;
/* Index of the next byte to be transmitted in gTxBuffer */
volatile uint8_t gTxIndex = 0;


/* Local function prototypes */
static void CopyArray(uint8_t *source, uint8_t *dest, uint8_t count);
static void CopyArraySwap(uint8_t *source, uint8_t *dest, uint8_t count);
static void SPI_Controller_writeReg(
    uint8_t writeCmd, uint8_t *data, uint8_t count);
static void SPI_Controller_readReg(uint8_t readCmd, uint8_t count);


uint16_t allLights[] =  {0x0, 0x0, 0xE2F0, 0x1010, 0xE210, 0x10F0, 0xE210, 0xF010, 0xE210, 0x0010, 0xFFFF, 0xFFFF};
uint16_t *currentStateSPIData;

int transmissionComplete = 0;
int idx = 0;
int message_len = sizeof(allLights) / sizeof(allLights[0]);


uint8_t gCmdReadType1Buffer[1] = {0};

uint8_t accelBuffer[6];

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} ACCEL;

ACCEL currentAccel;

float gravity[3];

float get_hypot(float * values, int count)
{
    float value = 0;
    for (int i = 0; i < count ; i++)
    {
        value += values[i]*values[i];
    }
    return sqrtf(value);
}

void normalize_float_array(float * array, int size)
{
    float hypot = get_hypot(array, size);
    for (int i = 0; i < size; i++)
    {
        array[i] /=hypot;
    }
}

float dot_product(float * v1, float* v2, int size)
{
    float val = 0;
    for (int i = 0; i < size; i++)
        val += v1[i]*v2[i];
    return val;
}


const float dt = 1.0 / 30.0;
const float gravity_multi = ( 1.0 / 500.0 );

const int pixel_buffer_face = 8*8;
const int led_grid_face = 10*10;

void fill_pixel_buffer()
{
    float *pixel_buffer_head = pixel_buffer;
    for (int i = 0; i < 6; i++)
    {
        for (int y = 0; y < 8; y++)
        {
            int odd = (y & 0x1) == 1;
            for (int x = 0; x < 8; x++)
            {
                *pixel_buffer_head = led_grid[led_grid_face * i + y*10 + 10 + odd * (9-x) + (!odd) * (x+1)];
                pixel_buffer_head++;
            }
        }
    }
}

int main(void)
{
    InitializeProcessor();
    InitializeGPIO();
    Initialize_LED_SPI();

    NVIC_EnableIRQ(SPI1_INT_IRQn);
    Initialize_ACCEL_SPI();

    uint8_t whoami;

    SPI_Controller_readReg(0x8F, 1);  // Read the who_am_i buffer should be 0x41
    CopyArray(gRxBuffer, &whoami, 1); // We could check it if we want!

    uint8_t enableCmd = 0x3F; // The value to enable all axes of the accelerometer with 100 Hz sampling
    SPI_Controller_writeReg(0x20, &enableCmd, 1); // The address of the CTRL1 reg is 0x20

    initialize_particles();

    /* Toggle LEDs*/
    while (1) {
        
        
        SPI_Controller_readReg(0xA8, 6); // Read from accelerometer starting at 0x28
                                         // 0xA8 is 0x28 with the MSB "Read" bit set
                                         // Take advantage of the multibyte transfer option
        CopyArray(gRxBuffer, (uint8_t *)&currentAccel, 6); // Copy into a type cast struct
                                                           // This works because uint16_t x,y,z
                                                           // are little endian!!!
        float ax = (float) -currentAccel.x * gravity_multi;
        float ay = (float) -currentAccel.z * gravity_multi;
        float az = (float) currentAccel.y * gravity_multi;

        float rx = -az;
        float ry = ax;
        float rz = ay;

//        __BKPT();

        tick_particles(dt, 50, rx, ry, rz);
        fill_pixel_buffer();
		

        update_neopixel_lights();
        delay_cycles(1600000);

    }
}

void SPI1_IRQHandler(void)
{
    uint8_t dump;

    switch (SPI1->CPU_INT.IIDX) {
        case SPI_CPU_INT_IIDX_STAT_TX_EVT:
            switch (gControllerMode) {
                case IDLE_MODE:
                case TIMEOUT_MODE:
                case WRITE_COMPLETE_MODE:
                    break;
                case TX_REG_ADDRESS_READ_MODE:
                case READ_DATA_MODE:
                    if (gTxByteCount) {
                        /*  Send dummy data to get read more bytes */
                        SPI1->TXDATA = DUMMY_DATA;
                        gTxByteCount--;
                    }
                    break;
                case TX_REG_ADDRESS_WRITE_MODE:
                    gControllerMode = WRITE_DATA_MODE;
                case WRITE_DATA_MODE:
                    if (gTxByteCount) {
                        /* Transmit data until all expected data is sent */
                        SPI1->TXDATA = gTxBuffer[gTxIndex++];
                        gTxByteCount--;
                    } else {
                        /* Transmission is done, prepare to reset state machine */
                        gControllerMode = WRITE_COMPLETE_MODE; // IDLE_MODE;
                    }
                    break;
            }
            break;
        case SPI_CPU_INT_IIDX_STAT_RX_EVT:
            switch (gControllerMode) {
                case IDLE_MODE:
                case TIMEOUT_MODE:
                    break;
                case TX_REG_ADDRESS_READ_MODE:
                    /* Ignore data and change state machine to read data */
                    dump = SPI1->RXDATA;
                    gControllerMode = READ_DATA_MODE;
                    break;
                case READ_DATA_MODE:
                    if (gRxByteCount) {
                        /* Receive data until all expected data is read */
                        gRxBuffer[gRxIndex++] = SPI1->RXDATA;
                        gRxByteCount--;
                    }
                    if (gRxByteCount == 0) {
                        /* All data is received, reset state machine */
                        gControllerMode = IDLE_MODE;
                    }
                    break;
                case TX_REG_ADDRESS_WRITE_MODE:
                case WRITE_DATA_MODE:
                    /* Ignore the data while transmitting */
                    dump = SPI1->RXDATA;
                    break;
                case WRITE_COMPLETE_MODE:
                    /* Ignore the data while transmitting */
                    dump = SPI1->RXDATA;
                    if (gTxByteCount == 0) {
                        // No more interrupts expected!
                        gControllerMode = IDLE_MODE;
                    }
                    break;
            }
            break;
        default:
            break;
    }
}

/*
 *  Controller sends a command to the Peripheral device to write data sent
 *  from the Controller.
 *  After sending the command, the Controller mode will be in
 *  TX_REG_ADDRESS_WRITE_MODE. If there are remaining bytes to transmit, the
 *  Controller will move to TX_DATA_MODE and it will continue to transmit
 *  the remaining bytes. Data sent by the Peripheral will be ignored during the
 *  transmission of the data. After all data is transmitted, the Controller
 *  will move back to IDLE_MODE.
 *
 *  writeCmd  The write command/register address to send to the Peripheral.
 *            Example: CMD_WRITE_TYPE_0
 *  data      The buffer containing the data to send to the Peripheral to write.
 *            Example: gCmdWriteType2Buffer
 *  count     The length of data to read. Example: TYPE_0_LENGTH
 */
static void SPI_Controller_writeReg(
    uint8_t writeCmd, uint8_t *data, uint8_t count)
{
    uint8_t dummy;

    gControllerMode = TX_REG_ADDRESS_WRITE_MODE;

    /* Copy data to gTxBuffer */
    CopyArray(data, gTxBuffer, count);

    gTxByteCount = count;
    gRxByteCount = count;
    gRxIndex     = 0;
    gTxIndex     = 0;

    /*
     * TX interrupts are disabled and RX interrupts are enabled by default.
     * TX interrupts will be enabled after sending the command, and they will
     * trigger after the FIFO has more space to send all subsequent bytes.
     */
    SPI1->CPU_INT.ICLR = SPI_CPU_INT_IMASK_TX_SET; // Clear TX interrupt status

    SPI1->TXDATA = writeCmd;
    SPI1->CPU_INT.IMASK |= SPI_CPU_INT_IMASK_TX_SET; // Enable TX interrupts

    /* Go to sleep until all data is transmitted */
    while (gControllerMode != IDLE_MODE) {
        __WFI();
    }

    /* Disable TX interrupts after the command is complete */
    SPI1->CPU_INT.IMASK &= ~(SPI_CPU_INT_IMASK_TX_SET);
    SPI1->CPU_INT.ICLR = SPI_CPU_INT_IMASK_TX_SET; // Clear TX interrupt status
}

/*
 *  Controller sends a command to the Peripheral device to read data as
 *  specified by the command and send it to the Controller.
 *  After sending the command, the Controller will be in
 *  TX_REG_ADDRESS_READ_MODE. After receiving the first byte from the
 *  Peripheral, if there are more bytes to receive, then the Controller will
 *  move to RX_DATA_MODE. The Controller will transmit DUMMY_DATA to
 *  receive 'count' number of bytes from the Peripheral.
 *  After 'count' number of bytes have been received, the Controller will move
 *  back to IDLE_MODE. The received data will be available in gRxBuffer.
 *
 *  readCmd   The read command/register address to send to the Peripheral.
 *            Example: CMD_WRITE_TYPE_0
 *  count     The length of data to read. Example: TYPE_0_LENGTH
 */
static void SPI_Controller_readReg(uint8_t readCmd, uint8_t count)
{
    gControllerMode = TX_REG_ADDRESS_READ_MODE;
    gRxByteCount    = count;
    gTxByteCount    = count;
    gRxIndex        = 0;
    gTxIndex        = 0;

    /*
     * TX interrupts are disabled and RX interrupts are enabled by default.
     * TX interrupts will be enabled after sending the command, and they will
     * trigger after the FIFO has more space to send all subsequent bytes.
     */
    SPI1->CPU_INT.ICLR = SPI_CPU_INT_IMASK_TX_SET; // Clear TX interrupt status

    SPI1->TXDATA = readCmd;
    SPI1->CPU_INT.IMASK |= SPI_CPU_INT_IMASK_TX_SET; // Enable TX interrupts

    /* Go to sleep until all data is transmitted */
    while (gControllerMode != IDLE_MODE) {
        __WFI();
    }

    /* Disable TX interrupts after the command is complete */
    SPI1->CPU_INT.IMASK &= ~(SPI_CPU_INT_IMASK_TX_SET);
    SPI1->CPU_INT.ICLR = SPI_CPU_INT_IMASK_TX_SET; // Clear TX interrupt status
}

/*
 * Copies an array from source to destination
 *
 *  source   Pointer to source array
 *  dest     Pointer to destination array
 *  count    Number of bytes to copy
 *
 */
static void CopyArray(uint8_t *source, uint8_t *dest, uint8_t count)
{
    uint8_t copyIndex = 0;
    for (copyIndex = 0; copyIndex < count; copyIndex++) {
        dest[copyIndex] = source[copyIndex];
    }
}


/*
 * Copyright (c) 2023, Caleb Kemere
 *
 * Modified (with bugs fixed) from example code provided by TI.
 *
 * Copyright (c) 2020, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

