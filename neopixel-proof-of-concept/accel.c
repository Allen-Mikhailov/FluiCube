#include <ti/devices/msp/msp.h>
#include "accel.h"
#include "simon_helper.h"


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
void init_accel()
{
//    InitializeProcessor();
//    InitializeGPIO();

    NVIC_EnableIRQ(SPI0_INT_IRQn);
    Initialize_ACCEL_SPI();

    uint8_t whoami;

    while(1)
    {
        SPI_Controller_readReg(0x75, 1);  // Read the who_am_i buffer should be 0x4E
        CopyArray(gRxBuffer, &whoami, 1); // We could check it if we want!

        delay_cycles(4000000);
    }



    if (whoami ==  0x4E)
        while (1)
        {
            // Loop inf
        }

    while (1)
    {
        // Loop inf
    }
}

void SPI0_IRQHandler(void)
{
    uint8_t dump;

    switch (SPI0->CPU_INT.IIDX) {
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
                        SPI0->TXDATA = DUMMY_DATA;
                        gTxByteCount--;
                    }
                    break;
                case TX_REG_ADDRESS_WRITE_MODE:
                    gControllerMode = WRITE_DATA_MODE;
                case WRITE_DATA_MODE:
                    if (gTxByteCount) {
                        /* Transmit data until all expected data is sent */
                        SPI0->TXDATA = gTxBuffer[gTxIndex++];
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
                    dump = SPI0->RXDATA;
                    gControllerMode = READ_DATA_MODE;
                    break;
                case READ_DATA_MODE:
                    if (gRxByteCount) {
                        /* Receive data until all expected data is read */
                        gRxBuffer[gRxIndex++] = SPI0->RXDATA;
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
                    dump = SPI0->RXDATA;
                    break;
                case WRITE_COMPLETE_MODE:
                    /* Ignore the data while transmitting */
                    dump = SPI0->RXDATA;
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
    SPI0->CPU_INT.ICLR = SPI_CPU_INT_IMASK_TX_SET; // Clear TX interrupt status

    SPI0->TXDATA = writeCmd;
    SPI0->CPU_INT.IMASK |= SPI_CPU_INT_IMASK_TX_SET; // Enable TX interrupts

    /* Go to sleep until all data is transmitted */
    while (gControllerMode != IDLE_MODE) {
        __WFI();
    }

    /* Disable TX interrupts after the command is complete */
    SPI0->CPU_INT.IMASK &= ~(SPI_CPU_INT_IMASK_TX_SET);
    SPI0->CPU_INT.ICLR = SPI_CPU_INT_IMASK_TX_SET; // Clear TX interrupt status
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
#define SPI_READ 0x80
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
    SPI0->CPU_INT.ICLR = SPI_CPU_INT_IMASK_TX_SET; // Clear TX interrupt status

    SPI0->TXDATA = readCmd | SPI_READ;
    SPI0->CPU_INT.IMASK |= SPI_CPU_INT_IMASK_TX_SET; // Enable TX interrupts

    /* Go to sleep until all data is transmitted */
    while (gControllerMode != IDLE_MODE) {
        __WFI();
    }

    /* Disable TX interrupts after the command is complete */
    SPI0->CPU_INT.IMASK &= ~(SPI_CPU_INT_IMASK_TX_SET);
    SPI0->CPU_INT.ICLR = SPI_CPU_INT_IMASK_TX_SET; // Clear TX interrupt status
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
