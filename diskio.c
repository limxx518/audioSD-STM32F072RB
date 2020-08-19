#include <stdio.h>
#include "stm32f0xx.h"
#include "stm32f0xx_conf.h"
#include "diskio.h"
#include "SD_spi.h"

static volatile DSTATUS Stat = STA_NOINIT;

DSTATUS disk_status(BYTE drv){
  if(drv) return STA_NOINIT;        //Supports only drive 0
  return Stat;                      //Return disk status
}

DSTATUS disk_initialize(BYTE drv){        //Notes: Ignore implementation for card detect and write protcect for now...
  INT_INIT_STATUS disk_status;
  
  if(drv) return STA_NOINIT;              //TODO: Implement timeout function using interrupt
  disk_status = init_SD();
  
  switch(disk_status){
  case CARD_SUCCESS: printf("SD card initialized\n");
                     Stat &= ~STA_NOINIT;
                     return Stat;
       CMD0_FAIL: printf("CMD0 failed\n");
                  Stat = STA_NOINIT;
                  break;
       CMD8_FAIL: printf("CMD8 failed\n");
                  Stat = STA_NOINIT;
                  break;
       CMD58_FAIL: printf("CMD58 failed\n");
                   Stat = STA_NOINIT;
                   break;
       CMD58_FAIL_2: printf("Card not initialized correctly after ACMD41\n");
                     Stat = STA_NOINIT;
                     break;
       CMD55_FAIL: printf("CMD55 failed\n");
                   Stat = STA_NOINIT;
                   break;
       CMD41_FAIL: printf("CMD41 failed\n");
                   Stat = STA_NOINIT;
                   break;
       default: printf("Card failed init\n");
                Stat = STA_NOINIT;
                break;
  }
  return Stat;
}

DRESULT disk_read(
   BYTE drv,           //Physical drive number (0)
   BYTE *buff,         //Pointer to data buffer to store read data
   LBA_t sector,       //Start sector number (LBA)
   UINT count          //Number of sectors to read (1...128) 
   )             
{
   uint8_t check;
   DWORD sect = (DWORD)sector;

   if(drv || !count) return RES_PARERR;
   if(Stat & STA_NOINIT) return RES_NOTRDY;

   sect *= 512;     //LBA to BA conversion

   if(count == 1){
      sendCMD(SPIx, CMD17, sector, 0x00);
      do{
        check = SPI_xfer(SPIx, 0xFF);
      }
      while(check);                     //TODO: Add timeout feature
      if(!read_single_block(buff, 512))
        count = 0;
   }
   else{
      sendCMD(SPIx, CMD18, sector, 0x00);
      do{
        check = SPI_xfer(SPIx, 0xFF);
      }
      while(check);                     //TODO: Add timeout feature
      do{
        if(read_single_block(buff, 512)) break;     //Error reading sector
        buff += 512;      //No errror, increment buffer address by another block
      }
      while(--count);
      sendCMD(SPIx, CMD12, 0, 0x00);
      SPI_xfer(SPIx, 0xFF);
      for(int k=0; k<10; k++){
        check = SPI_xfer(SPIx, 0xFF);
        if(check == (uint8_t) 0x80) break;
      }
   }
   return count ? RES_ERROR : RES_OK;
}