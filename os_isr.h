// os_isr.h
// Andrew Lynch
// Matthew DeKoning

#ifndef __OS_ISR
#define __OS_ISR

#include <stdint.h>
#include "FIFO.h"
#include "os.h"

#define ISR_STACK OFF

//******** OS_AddPeriodicThread *************** 
// add a background periodic task
// typically this function receives the highest priority
// Inputs: pointer to a void/void background function
//         period given in system time units (12.5ns)
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// You are free to select the time resolution for this function
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal	 OS_AddThread
// This task does not have a Thread ID
// In lab 2, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, this command will be called 0 1 or 2 times
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
uint8_t OS_AddPeriodicThread(void(*thread)(void), unsigned long period);

void ADC0_InitTimer0ATriggerSeq3(uint8_t channelNum, uint32_t period, void(*function)(uint16_t));
uint16_t ADC0Seq3_Get(void);
#endif // __OS_ISR
