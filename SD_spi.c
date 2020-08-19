#include "SD_spi.h"


uint8_t SPI_xfer(SPI_TypeDef* SPIX, uint8_t data){
  SPI_SendData8(SPIx, data);
  while(SPIx->SR & SPI_SR_BSY);
  return SPI_ReceiveData8(SPIx);
  //return (uint8_t) SPIx->DR;
}

void sendCMD(SPI_TypeDef* SPIX, uint8_t CMD, uint32_t arg, uint8_t crc){
  SPI_xfer(SPIx, CMD);
  SPI_xfer(SPIx, (uint8_t)(arg>>24));
  SPI_xfer(SPIx, (uint8_t)(arg>>16));
  SPI_xfer(SPIx, (uint8_t)(arg>>8));
  SPI_xfer(SPIx, (uint8_t)(arg));
  SPI_xfer(SPIx, crc);
}

uint8_t read_single_block(BYTE *buff, UINT btr){
   uint8_t check;
   do{
      check = SPI_xfer(SPIx, 0xFF);
   }
   while(check != 0xFE);
   if(check == 0xFE){         //start token received
      for(int k=0; k<btr; k++){
          buff[k] = SPI_xfer(SPIx, 0xFF);
      }
      check = SPI_xfer(SPIx, 0xFF);     //Flush out CRC
      check = SPI_xfer(SPIx, 0xFF);
      return 0;
   }
   else return 1;
}

void delay(uint16_t ms){
  uint16_t count=0;
  TIM_ClearFlag(TIM14, TIM_FLAG_Update);
  TIM_SetCounter(TIM14, 0x00000000);
  TIM_Cmd(TIM14, ENABLE);
  while(count < ms){
    while(TIM_GetFlagStatus(TIM14, TIM_FLAG_Update) != SET);
    TIM_ClearFlag(TIM14, TIM_FLAG_Update);
    count++;
  }
  TIM_Cmd(TIM14, DISABLE);
}

INT_INIT_STATUS init_SD(void){
  /* Place SD card in SPI mode by setting MOSI and CS lines to logic 1 and toggle SCK >= 74 cycles
     We set this toggling to 80 cycles -> 10 packets of 0xFF. Then set CS line to logic 0 and send CMD0     */
  uint8_t check;
  GPIO_SetBits(SPI_PORT, NSS_PIN);
  check = GPIO_ReadInputDataBit(SPI_PORT, NSS_PIN);
  uint8_t i = 0;
  while(i < 10){
    SPI_xfer(SPIx, 0xFF);
    i++;
  }
  i = 0;
  //Now set CS line low by resetting the NSS pin using GPIO function. This is 
  //because software NSS management SSM=1 is used
  GPIO_ResetBits(SPI_PORT, NSS_PIN);
  check = GPIO_ReadInputDataBit(SPI_PORT, NSS_PIN);

  //Now poll the SPI MISO pin to see if the card has driven its DO line high
  //while(!GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6) );

  //Sending CMD0: packet structure is 48 bits (6 byte packets)
  sendCMD(SPIx, CMD0, 0x00000000, 0x95);

  //Now, poll SD card to see if we get valid response of 0x01. To do this, keep sending junk 0xFF to toggle the SCK
  do{
    check = SPI_xfer(SPI1, 0xFF);
  }
  while(check != (uint8_t)0x01);

  i=0;

  //Now send CMD8 to determine version of SD card
  sendCMD(SPI1, CMD8, ARG_CMD8, CRC_CMD8);
  do{
    check = SPI_xfer(SPI1, 0xFF);
  }
  while(check != (uint8_t)0x01);
  for(int k=0; k<4; k++){
    check = SPI_xfer(SPI1, 0xFF);  //Expected sequence: 0x00, 0x00, 0x01, 0xAA (Echo back of arg)
    switch(k){
    case 0: if(!check);
            else return CMD8_FAIL;
            break;
    case 1: if(!check);
            else return CMD8_FAIL;
            break;
    case 2: if(check == (uint8_t)0x01);
            else return CMD8_FAIL;
            break;
    case 3: if(check == (uint8_t)0xAA);
            else return CMD8_FAIL;
            break;
    }
  }
  
  //Also for debugging purpose, see if card responds to CMD58
  sendCMD(SPI1, CMD58, 0x00000000, 0x00);
  do{
    check = SPI_xfer(SPI1, 0xFF);
  }
  while(check);
  for(int k=0; k<4; k++){
    check = SPI_xfer(SPI1, 0xFF);  //Expected sequence: 0xff, 0x80, 0x00, 0xff
    switch(k){
    case 0: if(check == (uint8_t)0xff);
            else return CMD58_FAIL;
            break;
    case 1: if(check == (uint8_t)0x80);
            else return CMD58_FAIL;
            break;
    case 2: if(!check);
            else return CMD58_FAIL;
            break;
    case 3: if(check == (uint8_t)0xff);
            else return CMD58_FAIL;
            break;
    }
  }

  do{
  //Verified that SD card is working. Now, start the SD card initialization process by sending CMD55 followed by CMD41
  sendCMD(SPI1, CMD55, 0x00000000, 0x00);
  do{
    check = SPI_xfer(SPI1, 0xFF);
    i++;
  }
  while(check > (uint8_t) 0x01 && i<5);
  if(i>=5) return CMD55_FAIL;
  //while(check);

  i=0;

  sendCMD(SPI1, CMD41, 0x40000000, 0x00); 
  delay(500); 
  do{
    check = SPI_xfer(SPI1, 0xFF);
    i++;
  }
  while(check > (uint8_t) 0x01 && i < 5);
  if(i>=5) return CMD41_FAIL;
  i=0;
  }
  while(check);

  //Now, check if operating voltage is correct using CMD58 
  sendCMD(SPI1, (0x40|58), 0x00000000, 0x00);
  do{
    check = SPI_xfer(SPI1, 0xFF);
  }
  while(check);
  for(int k=0; k<4; k++){
    check = SPI_xfer(SPI1, 0xFF);   //see if CCS bit is set, if set it's a SDHC card, exp resp: 0xc0, 0xFF, 0x80, 0x00
    switch(k){
    case 0: if(check == (uint8_t)0xc0);       //This code is written for SDHC card.
            else return CMD58_FAIL_2;
            break;
    case 1: if(check == (uint8_t)0xff);
            else return CMD58_FAIL_2;
            break;
    case 2: if(check == (uint8_t)0x80);
            else return CMD58_FAIL_2;
            break;
    case 3: if(!check);
            else return CMD58_FAIL_2;
            break;
    }
  }  
  //Success!
  return CARD_SUCCESS;
}
