#include "init.h"
#include "SD_spi.h"

//Initialization struct declarations
SPI_InitTypeDef SPI_InitStruct;
GPIO_InitTypeDef GPIO_InitStruct;
TIM_TimeBaseInitTypeDef TIM_InitStruct;
DAC_InitTypeDef DAC_InitStruct;
DMA_InitTypeDef DMA_InitStruct;

void init_peripherals(void){
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
  //NOTE:From reference manual Fig11, if APB prescaler != 1, timer clock freq x2 of APB domain
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
}

void setup_audio_peripherals(uint32_t mem_base_addr, uint32_t sampleRate){
  //Firstly, enable the clocks for DMA, TIM6 and DAC
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);

  //Init DMA
  DMA_DeInit(DMA1_Channel3);
  //Set the DMA->CPAR register to hold the address of the DAC_DHR12L1 register, which we are writing to
  DMA_InitStruct.DMA_PeripheralBaseAddr = DAC_BASE + (uint32_t)0x0000000C; //From ref manual  
  //Set the DMA->CMAR register to hold the address of wavbufffer1/wavBuffer2, which we'll grab raw wav data from
  DMA_InitStruct.DMA_MemoryBaseAddr = mem_base_addr;
  DMA_InitStruct.DMA_DIR = DMA_DIR_PeripheralDST;
  DMA_InitStruct.DMA_BufferSize = 512;
  DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
  DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;
  //The wav file we would be reading from is a stereo file. To play it as mono, read 32 bits from wavBuffer 
  //and discard the upper 16 bits, refer to ST reference manual Table 27. Set DAC DHR register as left-aligned tto grab 12
  //upper bits from the wav data
  DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
  DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
  DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;
  DMA_InitStruct.DMA_Priority = DMA_Priority_Low;     //Only one DMA channel here
  DMA_InitStruct.DMA_M2M = DMA_M2M_Disable;
  DMA_Init(DMA1_Channel3, &DMA_InitStruct);
  //Enable the DMA
  DMA_Cmd(DMA1_Channel3, ENABLE);

  //Init TIM16 to have the frequency equal to the sampling rate of the wav file
  //Note the tradeoff between timer granularity and range base of timer, the below statements find
  //the best granularity for the desired timing frequency
  //NOTE:The frequency of CK_PSC will be at 1MHz due to the APB prescaler != 1 as referenced under init_peripherals() called earlier
  //TODO: Make a conditional statement to replace 1Mhz value for CK_PSC below
  TIM_DeInit(TIM16);                                 
  TIM_Cmd(TIM16, DISABLE);
  TIM_InitStruct.TIM_Prescaler = 1000000/(sampleRate * 65535);
  TIM_InitStruct.TIM_CounterMode = TIM_CounterMode_Up;
  TIM_InitStruct.TIM_Period = (sampleRate * 1000000) / TIM_InitStruct.TIM_Prescaler - 1;
  //Clock division and repetition counter bits not applicable for TIM6
  TIM_TimeBaseInit(TIM16, &TIM_InitStruct);
  //Clear the TIM16 update flag set by above function call(expected, not confirmed)
  //    -->Reason being that intended first DMA transfer to begin on first timer overflow
  TIM_ClearFlag(TIM16, TIM_FLAG_Update);
  //Select the update event as the trigger output for TIM6
  TIM_SelectOutputTrigger(TIM16, TIM_TRGOSource_Update);

  //Init DAC
  DAC_InitStruct.DAC_Trigger =  DAC_Trigger_T6_TRGO;
  DAC_InitStruct.DAC_WaveGeneration = DAC_WaveGeneration_None;
  DAC_InitStruct.DAC_OutputBuffer = DAC_OutputBuffer_Enable;    //TODO: Experiment the difference between having this buffer enabled/disabled
  DAC_Init(DAC_Channel_1, &DAC_InitStruct);
  //Specify the data holding register value for DAC
  DAC_SetChannel1Data(DAC_Align_12b_L, 0x00000000);
  //Enable the DMA peripheral request from DAC
  DAC_DMACmd(DAC_Channel_1, ENABLE);
  //Enable the TIM16 clock
  TIM_Cmd(TIM16, ENABLE);
}