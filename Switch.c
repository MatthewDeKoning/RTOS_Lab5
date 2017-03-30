// Switch.c
// Runs on TMC4C123
// Use GPIO in edge time mode to request interrupts on any
// edge of PF4 and start Timer0B. In Timer0B one-shot
// interrupts, record the state of the switch once it has stopped
// bouncing.
// Daniel Valvano
// May 3, 2015

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to Arm Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2015

 Copyright 2015 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */

// PF4 connected to a negative logic switch using internal pull-up (trigger on both edges)
#include <stdint.h>
#include <stdbool.h>
#include "../inc/tm4c123gh6pm.h"
#include "Switch.h"
#include "SysTick.h"
#include "os.h"
#define PF4                     (*((volatile uint32_t *)0x40025040))
#define PF0                     (*((volatile uint32_t *)0x40025004))
#define PORTF_ADDRESS 0x40025000

/*
void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode
*/


volatile static unsigned long PreviousPF0;
volatile static unsigned long PreviousPF4;
int PF0Priority;
int PF4Priority;
void (*TouchTaskPF0)(void);
void (*TouchTaskPF4)(void);
void (*ReleaseTaskPF0)(void);
void (*ReleaseTaskPF4)(void);

void static NullTask(void){
  return;
}

static void GPIOPF4Arm(void){
  GPIO_PORTF_ICR_R = 0x10;      // (e) clear flag4
  GPIO_PORTF_IM_R |= 0x10;      // (f) arm interrupt on PF4 *** No IME bit as mentioned in Book ***
  NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|(5<<21); // (g) priority 5
  NVIC_EN0_R = 0x40000000;      // (h) enable interrupt 30 in NVIC  
}

static void GPIOPF0Arm(void){
  GPIO_PORTF_ICR_R = 0x1;      // (e) clear flag4
  GPIO_PORTF_IM_R |= 0x1;      // (f) arm interrupt on PF0 *** No IME bit as mentioned in Book ***
  NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|(5<<21); // (g) priority 5
  NVIC_EN0_R = 0x40000000;      // (h) enable interrupt 30 in NVIC  
}

void Switch_Init(){  
  SYSCTL_RCGCGPIO_R |= 0x00000020; // (a) activate clock for port F
  while((SYSCTL_PRGPIO_R & 0x00000020) == 0){};
  GPIO_PORTF_DIR_R &= ~0x11;    // (c) make PF4 in (built-in button)
  GPIO_PORTF_AFSEL_R &= ~0x11;  //     disable alt funct on PF4
  GPIO_PORTF_DEN_R |= 0x11;     //     enable digital I/O on PF4   
  GPIO_PORTF_PCTL_R &= ~0x000F000F; // configure PF4 as GPIO
  GPIO_PORTF_AMSEL_R = 0;       //     disable analog functionality on PF
  GPIO_PORTF_PUR_R |= 0x11;     //     enable weak pull-up on PF4
  GPIO_PORTF_IS_R &= ~0x11;     // (d) PF4 is edge-sensitive
  GPIO_PORTF_IBE_R |= 0x11;     //     PF4 is both edges

  TouchTaskPF0 = NullTask;
  TouchTaskPF4 = NullTask;
  ReleaseTaskPF0 = NullTask;
  ReleaseTaskPF4 = NullTask;
  PreviousPF0 = 1;
  PreviousPF4 = 1;
    
  GPIOPF4Arm();
  GPIOPF0Arm();
}

void Switch_KillSwitchTask(uint8_t switchNumber, uint8_t PushOrRelease){
  switch(switchNumber){
    case SW1:
      if(PushOrRelease == Push){
        TouchTaskPF4 = NullTask;
      }
      else{
        ReleaseTaskPF4 = NullTask;
      }
      break;
    case SW2:
      if(PushOrRelease == Push){
        TouchTaskPF0 = NullTask;
      }
      else{
        ReleaseTaskPF0 = NullTask;
      }
      break;
  }
}


void Switch_AddSwitchTask(uint8_t switchNumber, uint8_t PushOrRelease, void(*task)(void)){
  switch(switchNumber){
    case SW1:
      if(PushOrRelease == Push){
        TouchTaskPF4 = task;
      }
      else{
        ReleaseTaskPF4 = task;
      }
      break;
    case SW2:
      if(PushOrRelease == Push){
        TouchTaskPF0 = task;
      }
      else{
        ReleaseTaskPF0 = task;
      }
      break;
  }
}

//interrupts on edge triggered switch 
void GPIOPortF_Handler(void){
  GPIO_PORTF_ICR_R = 0x11;      // (e) clear flag3 and flag0
  GPIO_PORTF_IM_R &= ~0x11;     // disarm interrupt on PF4 and PF0
  unsigned long time = OS_MsTime(); // Debounce
  OS_Delay1ms(2);
  if(PreviousPF4 != PF4){
    if(PreviousPF4){
      //touch
      OS_ActivateSwitch(SW1,Push);
    }
    else{
      OS_ActivateSwitch(SW1,Release);
    }
  }
  if(PreviousPF0 != PF0){
    if(PreviousPF0){
      OS_ActivateSwitch(SW2,Push);
    }
    else{
      OS_ActivateSwitch(SW2,Release);
    }
  }
  PreviousPF4 = PF4;
  PreviousPF0 = PF0;
  GPIOPF4Arm();
  GPIOPF0Arm();
}
  