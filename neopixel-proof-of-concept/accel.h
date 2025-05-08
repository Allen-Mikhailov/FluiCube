#ifndef ACCEL_H
#define ACCEL_H


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

/* Local function prototypes */
static void CopyArray(uint8_t *source, uint8_t *dest, uint8_t count);
static void CopyArraySwap(uint8_t *source, uint8_t *dest, uint8_t count);
static void SPI_Controller_writeReg(
    uint8_t writeCmd, uint8_t *data, uint8_t count);
static void SPI_Controller_readReg(uint8_t readCmd, uint8_t count);

void init_accel();
void SPI0_IRQHandler(void);
#endif
