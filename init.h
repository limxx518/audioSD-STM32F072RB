#include "stm32f0xx.h"
#include "stm32f0xx_conf.h"

//Function declarations
void init_peripherals(void);
void setup_audio_peripherals(uint32_t mem_base_addr, uint32_t sampleRate);