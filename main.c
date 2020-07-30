/*********************************************************************
*                    SEGGER Microcontroller GmbH                     *
*                        The Embedded Experts                        *
**********************************************************************
*                                                                    *
*            (c) 2014 - 2020 SEGGER Microcontroller GmbH             *
*                                                                    *
*           www.segger.com     Support: support@segger.com           *
*                                                                    *
**********************************************************************
*                                                                    *
* All rights reserved.                                               *
*                                                                    *
* Redistribution and use in source and binary forms, with or         *
* without modification, are permitted provided that the following    *
* conditions are met:                                                *
*                                                                    *
* - Redistributions of source code must retain the above copyright   *
*   notice, this list of conditions and the following disclaimer.    *
*                                                                    *
* - Neither the name of SEGGER Microcontroller GmbH                  *
*   nor the names of its contributors may be used to endorse or      *
*   promote products derived from this software without specific     *
*   prior written permission.                                        *
*                                                                    *
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND             *
* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,        *
* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF           *
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE           *
* DISCLAIMED.                                                        *
* IN NO EVENT SHALL SEGGER Microcontroller GmbH BE LIABLE FOR        *
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR           *
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT  *
* OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;    *
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF      *
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT          *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE  *
* USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH   *
* DAMAGE.                                                            *
*                                                                    *
**********************************************************************

-------------------------- END-OF-HEADER -----------------------------

File    : main.c
Purpose : Generic application start

*/

#include <stdio.h>
#include <stdlib.h>
#include "stm32f0xx_conf.h"
//#include "SEGGER_RTT_Conf.h"
#include "SEGGER_RTT.h"

#include "stm32f0xx.h"

/*  Private Defines   */

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

//Private variables
SPI_InitTypeDef SPI_InitStruct;
GPIO_InitTypeDef GPIO_InitStruct;
TIM_TimeBaseInitTypeDef TIM_InitStruct;

//Function declarations
void sendCMD(SPI_TypeDef* SPIx, uint8_t CMD, uint32_t arg, uint8_t crc);
uint8_t SPI_xfer(SPI_TypeDef* SPIx, uint8_t data);
void delay(uint16_t ms);
/*********************************************************************
*
*       main()
*
*  Function description
*   Application entry point.
*/
int main(void) {

  //Test out the printf() function on SEGGER_RTT
  //TODO: Add a #if DEBUG preprocessor wrapper around this block of code. 
  //Plan is to develop a separate UART console for actual usage.
  //SEGGER_RTT_Init();
  /*printf("printf() test...HELLO WORLD!\n");
  printf("Enter Y/y to acknowledge and continue program\n");
  char c;
  //c = SEGGER_RTT_GetKey();
  scanf("%c\n", &c);
  while(c != 'Y' || c != 'y');*/

  /* Steps taken to configure the SPI for SD card comm:
      1) Set APB prescaler to 16 (this is for SPI baud rate of 250kHz)
      2) Enable clock for the SPI peripheral and GPIO
      3) Configure GPIO pins for MOSI, MISO and SCK
      4) Configure the SPI1 peripheral  
      5) Enable the SPI1 peripheral                */

  GPIO_DeInit(SPI_PORT);               //Note: The GPIO_DeInit() function needs to be called before the RCC enable,
                                    //this is because the RCC for peripherals get disabled in this function.
  TIM_DeInit(TIM14);                                 
  TIM_Cmd(TIM14, DISABLE);

  //Configure APB peripheral to 16
  RCC_PCLKConfig(RCC_HCLK_Div16);

  //Initialize Timer 14 for general delay functions. Set it to overflow at 1ms
  //NOTE:From reference manual Fig10, if APB prescaler != 1, timer clock freq x2 of APB domain
  //-> Timer clock frequency will be at 1Mhz
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM14, ENABLE);
  TIM_InitStruct.TIM_Prescaler = 0x0000;
  TIM_InitStruct.TIM_CounterMode = TIM_CounterMode_Up;
  TIM_InitStruct.TIM_Period = ONE_MS;   //1MHz/1kHz = 1000
  TIM_TimeBaseInit(TIM14, &TIM_InitStruct);
  TIM_UpdateDisableConfig(TIM14, DISABLE);

   delay(250);   //Let SD card power up

  //Enable clock for SPI peripheral and GPIO
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);

  //Configure GPIO pins for MOSI, MISO and SCK, follow example provided by ST under SPI_Config() function

  GPIO_PinAFConfig(SPI_PORT, GPIO_PinSource5, GPIO_AF_0);    //SCK
  GPIO_PinAFConfig(SPI_PORT, GPIO_PinSource6, GPIO_AF_0);    //MISO
  GPIO_PinAFConfig(SPI_PORT, GPIO_PinSource7, GPIO_AF_0);    //MOSI 
  
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_DOWN;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_Level_3;

  //Configure for SPI1 SCK pin
  GPIO_InitStruct.GPIO_Pin = SCK_PIN;
  GPIO_Init(SPI_PORT, &GPIO_InitStruct);

  //Configure for SPI1 MISO pin
  GPIO_InitStruct.GPIO_Pin = MISO_PIN;
  GPIO_Init(SPI_PORT, &GPIO_InitStruct);

  //Configure for SPI1 MOSI pin
  GPIO_InitStruct.GPIO_Pin = MOSI_PIN;
  GPIO_Init(SPI_PORT, &GPIO_InitStruct);

  //Configure for SPI1 NSS pin
  //Note: AF configuration may be wrong since SSM=1 is used. Try Output mode
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStruct.GPIO_Pin = NSS_PIN;
  GPIO_Init(SPI_PORT, &GPIO_InitStruct);

  //Configure SPI1 peripheral
  SPI_I2S_DeInit(SPIx);
  SPI_InitStruct.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
  SPI_InitStruct.SPI_Mode = SPI_Mode_Master;
  SPI_InitStruct.SPI_DataSize = SPI_DataSize_8b;
  SPI_InitStruct.SPI_CPOL = SPI_CPOL_Low;
  SPI_InitStruct.SPI_CPHA = SPI_CPHA_1Edge;
  SPI_InitStruct.SPI_NSS = SPI_NSS_Soft;
  SPI_InitStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
  SPI_InitStruct.SPI_FirstBit = SPI_FirstBit_MSB;
  SPI_Init(SPIx, &SPI_InitStruct);

  //Configure RX threshold to be a byte
  SPI_RxFIFOThresholdConfig(SPIx, SPI_RxFIFOThreshold_QF);

  //Enable the SPI1 peripheral
  SPI_Cmd(SPIx, ENABLE);

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
  }
  
  //Also for debugging purpose, see if card responds to CMD58
  sendCMD(SPI1, CMD58, 0x00000000, 0x00);
  do{
    check = SPI_xfer(SPI1, 0xFF);
  }
  while(check);
  for(int k=0; k<4; k++){
    check = SPI_xfer(SPI1, 0xFF);  //Expected sequence: 0xff, 0x80, 0x00, 0xff
  }

  do{
  //Verified that SD card is working. Now, start the SD card initialization process by sending CMD55 followed by CMD41
  sendCMD(SPI1, (0x40|0x37), 0x00000000, 0x00);
  do{
    check = SPI_xfer(SPI1, 0xFF);
    i++;
  }
  while(check > (uint8_t) 0x01 && i<5);
  //while(check);

  i=0;

  sendCMD(SPI1, (0x40|41), 0x40000000, 0x00); 
  delay(500); 
  do{
    check = SPI_xfer(SPI1, 0xFF);
    i++;
  }
  while(check > (uint8_t) 0x01 && i < 5);
  //check = SPI_xfer(SPI1, 0xFF);
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
  }

  //Finally got through initialization!! Since CCS bit is set, write operations use block addressing (LBA) Each 
  //block is 512 bytes. Try reading the first block with LBA = 0. Write out the results to COM terminal using UART
  //for debug.
  

  //Command for single block read is CMD17. 
  uint8_t block[512];
  sendCMD(SPI1, (0x40|17), 0x00000000, 0x00);
  do{
    check = SPI_xfer(SPI1, 0xFF);
  }
  while(check);
  //Now poll response from the card until a token is received
  do{
    check = SPI_xfer(SPI1, 0xFF);
  }
  while(check == (uint8_t) 0xFF);
  if(check == 0xFE){
    for(int k=0; k<512; k++){
      block[k] = SPI_xfer(SPI1, 0xFF);
    }
  }
  //flush out the two CRC bits
  check = SPI_xfer(SPI1, 0xFF);
  check = SPI_xfer(SPI1, 0xFF);

  //Stuff written up to now are based on values probed using the debugger. Formal initialization 
  //code written later must include more condition checking based on expected values

  //Based on documentation from PJRC, boot code in the first sector (MBR) is 446 bytes. We just need 
  //to concern ourselves with Partition 1 (16 bytes) at the moment. Therefore we look at Partition 1
  //starting from block[446]. Type code which is the fifth byte (block[450]) tells us if FAT32 is used
  //(values 0x0B or 0x0c). Of most importance to use if the LBA begin, which tells us where the first FAT volume 
  //begins. LBA begin is 4 bytes long, starting from the 9th to 12th byte (block[454] - block[457])

  uint32_t addr = block[457] << 24 | block[456] << 16 | block[455] << 8 | block[454];
  //Some condition checking
  if(block[450] == (uint8_t)0x0B || block[450] == (uint8_t)0x0C){
    //read volume ID of first FAT partition
    //Volume ID is found from the LBA of the MBR and stored in little-endian format
    //Wikipedia's entry on endianness has a good illustration on how a number is stored
    //in memory for both types of endianness.
    sendCMD(SPI1, (0x40|17), addr, 0x00);
    do{
      check = SPI_xfer(SPI1, 0xFF);
    }
    while(check);
    //Now poll response from the card until a token is received
    do{
      check = SPI_xfer(SPI1, 0xFF);
    }
   while(check == (uint8_t) 0xFF);
   if(check == 0xFE){
    for(int k=0; k<512; k++){
      block[k] = SPI_xfer(SPI1, 0xFF);
    }
  }
  //flush out the two CRC bits
  check = SPI_xfer(SPI1, 0xFF);
  check = SPI_xfer(SPI1, 0xFF);
  }

  //Calculate values for the below variables to access the FAT32 filesystem, based on PJRC reference
  uint8_t sectors_per_cluster = block[13];
  uint32_t root_dir_first_cluster = block[47]<<24 | block[46]<<16 | block[45]<<8 | block[44];
  uint16_t num_reserved_sectors = block[15]<<8 | block[14];
  uint32_t sectors_per_fat = block[39]<<24 | block[38]<<16 | block[37]<<8 | block[36];
  uint32_t fat_begin_lba = addr + num_reserved_sectors;
  uint32_t cluster_begin_lba = fat_begin_lba + (block[16] * sectors_per_fat);


  while(1);
}


void sendCMD(SPI_TypeDef* SPIx, uint8_t CMD, uint32_t arg, uint8_t crc){
  SPI_xfer(SPIx, CMD);
  SPI_xfer(SPIx, (uint8_t)(arg>>24));
  SPI_xfer(SPIx, (uint8_t)(arg>>16));
  SPI_xfer(SPIx, (uint8_t)(arg>>8));
  SPI_xfer(SPIx, (uint8_t)(arg));
  SPI_xfer(SPIx, crc);
}

uint8_t SPI_xfer(SPI_TypeDef* SPIx, uint8_t data){
  SPI_SendData8(SPIx, data);
  while(SPIx->SR & SPI_SR_BSY);
  return SPI_ReceiveData8(SPIx);
  //return (uint8_t) SPIx->DR;
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





/*************************** End of file ****************************/
