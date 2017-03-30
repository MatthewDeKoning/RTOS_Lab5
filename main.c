// main.c
// Andrew Lynch
// Matthew DeKoning
#include <stdint.h>
#include "PLL.h"
#include "../inc/tm4c123gh6pm.h"
#include "os.h"
#include "eDisk.h"
#include "ST7735.h"
#include "UART.h"
#include <stdint.h>
#include "Heap.h"
#include "interpreter.h"

BYTE buffer[512];
BYTE meta[4];
int * count1 = 0;
void Thread1(void){
  count1 = (int *) Heap_Malloc(4);
  while(1){
    (*count1)++;
    OS_Sleep(1);
  }
}

int * count2 = 0;
void Thread2(void){
  count2 = (int *) Heap_Malloc(4);
  while(1){
    (*count2)++;
  }
}
char message[600];
void GPIO_Init(void){
  SYSCTL_RCGCGPIO_R |= 0x20;        // 1) activate port E
  while((SYSCTL_PRGPIO_R&0x20)==0){};   // allow time for clock to stabilize
                                    // 2) no need to unlock PE3-0
  GPIO_PORTF_AMSEL_R &= ~0x06;      // 3) disable analog functionality on PE3-0
  GPIO_PORTF_PCTL_R &= ~0x00000FF0; // 4) GPIO
  GPIO_PORTF_DIR_R |= 0x06;         // 5) make PE3-0 out
  GPIO_PORTF_AFSEL_R &= ~0x06;      // 6) regular port function
  GPIO_PORTF_DEN_R |= 0x06;         // 7) enable digital I/O on PE3-0
}
#define PF1  (*((volatile uint32_t *)0x40025008))
#define PF2  (*((volatile uint32_t *)0x40025010))  

/*
static void print_file(char file[]){
  int size, i, j;
  char * ptr;
  j = 0;
  size = eFile_ROpen(file, &ptr);
  while(size != 0){
    for(i = 0; i < size; i++){
      
      UART_OutChar(*ptr);
      ptr++;
      
    }
    size = eFile_ReadNext(&ptr);
    ST7735_ds_OutString(j, "hello");
    ST7735_ds_OutString(j, ", bye");
     j++;
  }
}*/
int main1112(void){
  INTERPRETER_Run();
  
}