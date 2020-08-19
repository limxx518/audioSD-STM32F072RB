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
#include <string.h>
//#include "SEGGER_RTT_Conf.h"
//#include "SEGGER_RTT.h"

#include "stm32f0xx.h"
#include "stm32f0xx_conf.h"
#include "init.h"

/*  Private Defines   */

//Defines for WAV header
#define MONO      (uint16_t)0x0001
#define STEREO    (uint16_t)0x0002


//Struct for wav header
typedef struct{
  //RIFF header
  char riffHeader[5];       //Contains "RIFF"
  uint32_t fileSize;
  char waveHeader[5];       //Contains "WAVE"

  //Format header
  char fmtHeader[5];        //Contains "fmt ", note the trailing null
  uint32_t fmt_chunk_size;    //Should be 16 for PCM
  uint16_t audioFormat;       //Should be 1 for PCM
  uint16_t numChannels;       //1 for mono, 2 for stereo
  uint32_t sampleRate;
  uint32_t byteRate;        //sampleRate * numChannels * bytes_per_sample
  uint16_t blockAlign;      //Number of bytes in a frame -> numChannels * number of bytes/sample (Frame is a snapshot of all samples)
  uint16_t bits_per_sample;

  //Data header
  char dataHeader[5];       //Contains "data'
  uint32_t data_size;       //Number of bytes of sound data (numSamples * numChannels * bits_per_sample/8)
}WAV_HEADER;

//Private variables

uint32_t wavBuffer1[512];
uint32_t wavBuffer2[512];
WAV_HEADER wav_header;

#include "diskio.h"

FATFS fs;      //Pointer to the filesystem object

//Function declaration
FRESULT find_wav_file(char *path, char *file);
FRESULT parse_wav_file(FIL *fp, WAV_HEADER *wav_ptr);
/*********************************************************************
*
*       main()
*
*  Function description
*   Application entry point.
*/
int main(void){

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

  FRESULT fresult;
  char file[50];        //TODO: Make 50 a define later on when the program works properly
  //char wav_header[44];
  FIL fil;              //File object
  UINT bytes_read;

  //WAV_HEADER wav_header;

  printf("Starting program execution\n");
  //fs = (FATFS*)malloc(sizeof(FATFS));
  init_peripherals();
  //force mount and see if that works
  fresult = f_mount(&fs, "", 1);
  if(fresult == FR_OK)
    fresult = find_wav_file("", file);
  else  
    printf("Unable to mount SD card\n");

  if(fresult == FR_OK){ //Parse .wav header structure(44 bytes long)
    fresult = f_open(&fil, file, FA_READ);
    if(fresult) return (int) fresult;
    //Read first 44 bytes from wav file
    if(parse_wav_file(&fil, &wav_header) == FR_OK){      
      //Set up DMA and TIM6 peripheral, based on ST Application note AN3126
      //First read 512 units of data into wavBuffer1, units of data is defined depending on if
      //the file is mono or stereo. For stereo file and playing it as a mono, for 16 bits/sample and 32 bit element array
      //upper 16 bits are ignored. 
      //TODO: Use malloc() to dynamically define the size of wavBuffer arrays depending on bitsperSample and mono/stereo file
      printf("Size of wavBuffer1 is: %d\n", sizeof(wavBuffer1));
      fresult = f_read(&fil, &wavBuffer1, sizeof(wavBuffer1), &bytes_read);
      if(fresult == FR_OK && bytes_read == sizeof(wavBuffer1)){
        setup_audio_peripherals((uint32_t)&wavBuffer1, wav_header.sampleRate);
      }
    }
    /*fresult = f_read(&fil, wav_header, 44, &bytes_read);
    if(fresult != FR_OK || bytes_read != 44)
      printf("Error occurred while reading wav header\n");
    else printf("Success!\n");*/
  }
  while(1);
}
  

FRESULT parse_wav_file(FIL *fp, WAV_HEADER *wav_ptr){ //big-endian to little-endian conversion already done by FatFs module
  FRESULT fresult;
  UINT bytes_read;

  fresult = f_read(fp, &(wav_ptr->riffHeader), 4, &bytes_read);
  if(fresult != FR_OK || bytes_read != 4){
    printf("Error reading wav header\n");
    return 1;
  }
  wav_ptr->riffHeader[4] = NULL;
  if(strcmp(wav_ptr->riffHeader, "RIFF")){    
   printf("Not RIFF file\n");
   return 1;
   }
  //Now, read the file size
  fresult = f_read(fp, &(wav_ptr->fileSize), 4, &bytes_read);
  if(fresult != FR_OK || bytes_read != 4){
    printf("Error reading wav header\n");
    return 1;
  }
  //Now, read file type
  fresult = f_read(fp, &(wav_ptr->waveHeader), 4, &bytes_read);
  if(fresult != FR_OK || bytes_read != 4){
    //Package the above error function
    return 1;
  }
  wav_ptr->waveHeader[4] = '\0';
  if(strcmp(wav_ptr->waveHeader, "WAVE")){ 
    printf("Not wav file\n");
    return 1;
  }
  //Now, parse the format section of header
  fresult = f_read(fp, &(wav_ptr->fmtHeader), 4, &bytes_read);
  if(fresult != FR_OK || bytes_read != 4){
    return 1;
  }
  //Get the chunk size of the format section. Should be 16 for PCM
  fresult = f_read(fp, &(wav_ptr->fmt_chunk_size), 4, &bytes_read);
  if(fresult != FR_OK || bytes_read != 4){
    return 1;
  }
  if(wav_ptr->fmt_chunk_size != (uint32_t)16){
    printf("fmt subchunk size error\n");
    return 1;
  }
  //Get audio format
  fresult = f_read(fp, &(wav_ptr->audioFormat), 2, &bytes_read);
  if(fresult != FR_OK || bytes_read != 2){
    return 1;
  }
  if(wav_ptr->audioFormat != (uint16_t) 1){
    printf("Can't process uncompressed audio file\n");
    return 1;
  }
  //Get number of channels
  fresult = f_read(fp, &(wav_ptr->numChannels), 2, &bytes_read);
  if(fresult != FR_OK || bytes_read != 2){
    return 1;
  }
  //Get sample rate
  fresult = f_read(fp, &(wav_ptr->sampleRate), 4, &bytes_read);
  if(fresult != FR_OK || bytes_read != 4){
    return 1;
  }
  //Get byte rate
  fresult = f_read(fp, &(wav_ptr->byteRate), 4, &bytes_read);
  if(fresult != FR_OK || bytes_read != 4){
    return 1;
  }
  //Get block align
  fresult = f_read(fp, &(wav_ptr->blockAlign), 2, &bytes_read);
  if(fresult != FR_OK || bytes_read != 2){
    return 1;
  }
  //Get bits per sample
  fresult = f_read(fp, &(wav_ptr->bits_per_sample), 2, &bytes_read);
  if(fresult != FR_OK || bytes_read != 2){
    return 1;
  }
  //Now, parse the data subchunk of the wav header file
  fresult = f_read(fp, &(wav_ptr->dataHeader), 4, &bytes_read);
  if(fresult != FR_OK || bytes_read != 4){
    return 1;
  }
  wav_ptr->dataHeader[4] = '\0';
  //if(strcmp(wav_ptr->dataHeader, "data"))....
  //Get data size of the raw data
  fresult = f_read(fp, &(wav_ptr->data_size), 4, &bytes_read);
  if(fresult != FR_OK || bytes_read != 4){
    return 1;
  }
  //Now, we can finally read the raw data!
  return FR_OK;
}

FRESULT find_wav_file(char *path, char *file){
  FRESULT fr;
  DIR dj;
  static FILINFO fno;
  TCHAR str[100];  //TODO: If function is successful, 50 to be defined as SZ_STR
  UINT i;
  typedef enum{
    W_FOUND = 1,
    A_FOUND,
    V_FOUND
  }WAV;
  WAV state = 0;

  fr = f_opendir(&dj, path);
  if(fr == FR_OK){
    for(;;){
      fr = f_readdir(&dj, &fno);
      if(fr != FR_OK || fno.fname[0] == 0) break;
      if(!strcmp(fno.fname, "Android")) continue;
      if(fno.fattrib == AM_DIR){
        strcpy(str, path);
        i = strlen(path);
        sprintf(&str[i], "/%s", fno.fname);
        fr = find_wav_file(str, file);
        if(fr != FR_OK) break;
        str[i] = 0;
      }
      else{   //Not a directory, perform a simple pattern matching to see if file is a .wav extension
        char *ptr;
        ptr = fno.fname;
        while(ptr != NULL){
          while(*ptr != '.' && state != W_FOUND)
            ptr++;
          if(*(++ptr) == 'w' && state != W_FOUND)
            state = W_FOUND;
          else break;
          if(*(++ptr) == 'a' && state == W_FOUND)
            state = A_FOUND;
          else break;
          if(*(++ptr) == 'v' && state == A_FOUND){
            printf(".wav file extension found!\n");
            printf("File is %s\n", fno.fname);
            //strcpy(file, fno.fname);
            sprintf(file, "%s/%s", path, fno.fname);
            printf("%s\n", file);
            break;
          }
        }
      }
    }
  f_closedir(&dj);
  }
  return fr;
}


/*************************** End of file ****************************/
