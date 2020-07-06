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
//#include "stm32f0xx_conf.h"

#include "stm32f0xx.h"

//Private variables
SPI_InitTypeDef SPI_InitStruct;
GPIO_InitTypeDef GPIO_InitStruct;

/*********************************************************************
*
*       main()
*
*  Function description
*   Application entry point.
*/
int main(void) {

  /* Steps taken to configure the SPI for SD card comm:
      1) Set APB prescaler to 16 (this is for SPI baud rate of 250kHz)
      2) Enable clock for the SPI peripheral and GPIO
      3) Configure GPIO pins for MOSI, MISO and SCK
      4) Configure the SPI1 peripheral  
      5) Enable the SPI1 peripheral                */

  //Configure APB peripheral to 16
  RCC_PCLKConfig(RCC_HCLK_Div16);

  //Enable clock for SPI peripheral and GPIO
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);

  //Configure GPIO pins for MOSI, MISO and SCK, follow example provided by ST under SPI_Config() function
  GPIO_DeInit(GPIOA);

  /*//This block only for debuggging purposes, determining why value on certain pin cannot be set
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_DOWN;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_Level_3;
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_7;
  GPIO_Init(GPIOA, &GPIO_InitStruct);
  //Now try setting the bit on pin 7
  GPIO_SetBits(GPIOA, GPIO_Pin_7);
  uint8_t check;
  check = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_7);
  check = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4);*/
  
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_DOWN;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_Level_3;

  //Configure for SPI1 SCK pin
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_5;
  GPIO_Init(GPIOA, &GPIO_InitStruct);

  //Configure for SPI1 MISO pin
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6;
  GPIO_Init(GPIOA, &GPIO_InitStruct);

  //Configure for SPI1 MOSI pin
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_7;
  GPIO_Init(GPIOA, &GPIO_InitStruct);

  //Configure for SPI1 NSS pin
  //Note: AF configuration may be wrong since SSM=1 is used. Try Output mode
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_4;
  GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_PinAFConfig(GPIOA, GPIO_PinSource5, GPIO_AF_0);    //SCK
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource6, GPIO_AF_0);    //MISO
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_0);    //MOSI

  //Configure SPI1 peripheral
  SPI_I2S_DeInit(SPI1);
  SPI_InitStruct.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
  SPI_InitStruct.SPI_Mode = SPI_Mode_Master;
  SPI_InitStruct.SPI_DataSize = SPI_DataSize_8b;
  SPI_InitStruct.SPI_CPOL = SPI_CPOL_Low;
  SPI_InitStruct.SPI_CPHA = SPI_CPHA_1Edge;
  SPI_InitStruct.SPI_NSS = SPI_NSS_Soft;
  SPI_InitStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
  SPI_InitStruct.SPI_FirstBit = SPI_FirstBit_MSB;
  SPI_Init(SPI1, &SPI_InitStruct);

  //Configure RX threshold to be a byte
  SPI_RxFIFOThresholdConfig(SPI1, SPI_RxFIFOThreshold_QF);

  //Enable the SPI1 peripheral
  //SPI_Cmd(SPI1, ENABLE);

  /* Place SD card in SPI mode by setting MOSI and CS lines to logic 1 and toggle SCK >= 74 cycles
     We set this toggling to 80 cycles -> 10 packets of 0xFF. Then set CS line to logic 0 and send CMD0     */
  uint8_t check;
  GPIO_SetBits(GPIOA, GPIO_Pin_4);
  check = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4);
  uint8_t i = 0;
  while(i < 10){
    SPI_SendData8(SPI1, 0xFF);
    while(SPI1->SR & SPI_SR_BSY);
    //Flush RX buffer
    SPI_ReceiveData8(SPI1);
    i++;
  }
  i = 0;
  //Now set CS line low by resetting the NSS pin using GPIO function. This is 
  //because software NSS management SSM=1 is used
  GPIO_ResetBits(GPIOA, GPIO_Pin_4);
  check = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4);

  //Now poll the SPI MISO pin to see if the card has driven its DO line high
  //while(!GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6) );

  //Sending CMD0: packet structure is 48 bits (6 byte packets)
  SPI_SendData8(SPI1,0x40);
  while(SPI1->SR & SPI_SR_BSY);
  SPI_ReceiveData8(SPI1);

  SPI_SendData8(SPI1,0x00);
  while(SPI1->SR & SPI_SR_BSY);
  SPI_ReceiveData8(SPI1);

  SPI_SendData8(SPI1,0x00);
  while(SPI1->SR & SPI_SR_BSY);
  SPI_ReceiveData8(SPI1);

  SPI_SendData8(SPI1,0x00);
  while(SPI1->SR & SPI_SR_BSY);
  SPI_ReceiveData8(SPI1);

  SPI_SendData8(SPI1,0x00);
  while(SPI1->SR & SPI_SR_BSY);
  SPI_ReceiveData8(SPI1);

  SPI_SendData8(SPI1,0x95);
  while(SPI1->SR & SPI_SR_BSY);
  SPI_ReceiveData8(SPI1);

  //Now, poll SD card to see if we get valid response of 0x01. To do this, keep sending junk 0xFF to toggle the SCK
  do{
    SPI_SendData8(SPI1, 0xFF);
    while(SPI1->SR & SPI_SR_BSY);
    i++;
  }
  while(SPI_ReceiveData8(SPI1) != (uint8_t)0x01 && i < 16);

  i=0;

  //Try sending CMD8 to see if there's a response
  SPI_SendData8(SPI1, 0x48);
  while(SPI1->SR & SPI_SR_BSY);
  SPI_ReceiveData8(SPI1);

  SPI_SendData8(SPI1, 0x00);
  while(SPI1->SR & SPI_SR_BSY);
  SPI_ReceiveData8(SPI1);

  SPI_SendData8(SPI1, 0x00);
  while(SPI1->SR & SPI_SR_BSY);
  SPI_ReceiveData8(SPI1);

  SPI_SendData8(SPI1, 0x01);
  while(SPI1->SR & SPI_SR_BSY);
  SPI_ReceiveData8(SPI1);

  SPI_SendData8(SPI1, 0xAA);
  while(SPI1->SR & SPI_SR_BSY);
  SPI_ReceiveData8(SPI1);

  SPI_SendData8(SPI1, 0x87);
  while(SPI1->SR & SPI_SR_BSY);
  SPI_ReceiveData8(SPI1);

  do{
    SPI_SendData8(SPI1, 0xFF);
    while(SPI1->SR & SPI_SR_BSY);
  }
  while(!SPI_ReceiveData8(SPI1));

   while(1);
}


/*************************** End of file ****************************/
