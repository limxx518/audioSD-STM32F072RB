#include "stm32f0xx.h"
//#include "stm32f0xx_conf.h" 
#include "stm32f0xx_spi.h"
#include "diskio.h"

#define SPI_CH 1

#if SPI_CH == 1
#define SPI_PORT GPIOA
#define SPIx SPI1
#endif
 
#define ONE_MS          1000
#define SCK_PIN         GPIO_Pin_5
#define MISO_PIN        GPIO_Pin_6
#define MOSI_PIN        GPIO_Pin_7
#define NSS_PIN         GPIO_Pin_4

/* Defines for SD card commands   */
#define CMD0          (0x40|0)
#define CMD8          (0x40|0x08)
#define ARG_CMD8      (0x000001AA)
#define CRC_CMD8      (0x87)
#define CMD58         (0x40|58)
#define CMD55         (0x40|0x37)
#define CMD41         (0x40|41)
#define CMD17         (0x40|17)
#define CMD18         (0x40|18)
#define CMD12         (0x40|12)

//Function declarations
void sendCMD(SPI_TypeDef* SPIX, uint8_t CMD, uint32_t arg, uint8_t crc);
uint8_t SPI_xfer(SPI_TypeDef* SPIX, uint8_t data);
uint8_t read_single_block(BYTE *buff, UINT btr);
INT_INIT_STATUS init_SD(void);
void delay(uint16_t ms);