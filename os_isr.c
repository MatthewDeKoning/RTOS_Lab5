// os_isr.c
// Andrew Lynch
// Matthew DeKoning

#include "os.h"
#include "os_isr.h"
#include "SysTick.h"
#include "FIFO.h"
#include "memManager.h"
#include <stdio.h>
#include "../inc/tm4c123gh6pm.h"

void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode
extern void EnterISR(void);
extern int ExitISRasm(void);

uint32_t OSISR_Stack[100];
int32_t OSISR_NestingCtr = 1;

// private prototypes

void Timer1B_Init(void(*task)(void), unsigned long period);
void Timer2A_Init(void(*task)(void), unsigned long period);
void Timer2B_Init(void(*task)(void), unsigned long period);
void Timer3B_Init(void(*task)(void), unsigned long period);
void Timer4A_Init(void(*task)(void), unsigned long period);
void Timer4B_Init(void(*task)(void), unsigned long period);
void Timer5A_Init(void(*task)(void), unsigned long period);
void Timer5B_Init(void(*task)(void), unsigned long period);

// implementations

void ExitISR(void){
  if(ExitISRasm() && OSInterruptTask_R){
    OS_Suspend();
  }
}

static uint8_t RemainingPeriodicFunctions = 8;
uint8_t OS_AddPeriodicThread(void(*thread)(void), unsigned long period){
  if(!RemainingPeriodicFunctions){
    return 0;
  }
  else{
    switch(RemainingPeriodicFunctions){
      case 8:
        Timer2A_Init(thread, period);
        break;
      case 7:
        Timer2B_Init(thread, period);
        break;
      case 6:
        Timer1B_Init(thread, period);
        break;
      case 5:
        Timer3B_Init(thread, period);
        break;
      case 4:
        Timer4A_Init(thread, period);
        break;
      case 3:
        Timer4B_Init(thread, period);
        break;
      case 2:
        Timer5A_Init(thread, period);
        break;
      case 1:
        Timer5B_Init(thread, period);
        break;      
    }
    RemainingPeriodicFunctions--;
  }
  return 1;
}


void (*PeriodicTask1B)(void);   // user function
void (*PeriodicTask2A)(void);   // user function
void (*PeriodicTask2B)(void);   // user function
void (*PeriodicTask3B)(void);   // user function
void (*PeriodicTask4A)(void);   // user function
void (*PeriodicTask4B)(void);   // user function
void (*PeriodicTask5A)(void);   // user function
void (*PeriodicTask5B)(void);   // user function

void Timer1B_Handler(void){
  TIMER2_ICR_R = TIMER_ICR_TBTOCINT;// acknowledge TIMER1B timeout
  #if ISR_STACK
  EnterISR();
  #endif 
  (PeriodicTask1B)();                // execute user task
  #if ISR_STACK
  ExitISR();
  #endif
  EnableInterrupts();
}

void Timer2A_Handler(void){
  TIMER2_ICR_R = TIMER_ICR_TATOCINT;// acknowledge TIMER2A timeout
  #if ISR_STACK
  EnterISR();
  #endif 
  (*PeriodicTask2A)();                // execute user task
  #if ISR_STACK
  ExitISR();
  #endif
  EnableInterrupts();
}

void Timer2B_Handler(void){
  TIMER2_ICR_R = TIMER_ICR_TBTOCINT;// acknowledge TIMER2A timeout
  #if ISR_STACK
  EnterISR();
  #endif 
  (*PeriodicTask2B)();                // execute user task
  #if ISR_STACK
  ExitISR();
  #endif
  EnableInterrupts();
}

void Timer3B_Handler(void){
  TIMER2_ICR_R = TIMER_ICR_TBTOCINT;// acknowledge TIMER3B timeout
  #if ISR_STACK
  EnterISR();
  #endif 
  (*PeriodicTask3B)();                // execute user task
  #if ISR_STACK
  ExitISR();
  #endif
  EnableInterrupts();
}

void Timer4A_Handler(void){
  TIMER2_ICR_R = TIMER_ICR_TATOCINT;// acknowledge TIMER4A timeout
  #if ISR_STACK
  EnterISR();
  #endif 
  (*PeriodicTask4A)();                // execute user task
  #if ISR_STACK
  ExitISR();
  #endif
  EnableInterrupts();
}

void Timer4B_Handler(void){
  TIMER2_ICR_R = TIMER_ICR_TBTOCINT;// acknowledge TIMER4B timeout
  #if ISR_STACK
  EnterISR();
  #endif 
  (*PeriodicTask4B)();                // execute user task
  #if ISR_STACK
  ExitISR();
  #endif
  EnableInterrupts();
}

/*void Timer5A_Handler(void){
  TIMER2_ICR_R = TIMER_ICR_TATOCINT;// acknowledge TIMER5A timeout
  #if ISR_STACK
  EnterISR();
  #endif 
  (*PeriodicTask5A)();                // execute user task
  #if ISR_STACK
  ExitISR();
  #endif
  EnableInterrupts();
}*/

void Timer5B_Handler(void){
  TIMER2_ICR_R = TIMER_ICR_TBTOCINT;// acknowledge TIMER5B timeout
  #if ISR_STACK
  EnterISR();
  #endif 
  (*PeriodicTask5B)();                // execute user task
  #if ISR_STACK
  ExitISR();
  #endif
  EnableInterrupts();
}


// ***************** Timer1B_Init ****************
// Activate Timer1 interrupts to run user task periodically
// Inputs:  task is a pointer to a user function
//          period in units (1/clockfreq)
// Outputs: none
void Timer1B_Init(void(*task)(void), unsigned long period){
  long sr = StartCritical();
  SYSCTL_RCGCTIMER_R |= 0x02;   // 0) activate timer1
  PeriodicTask1B = task;          // user function
  TIMER1_CTL_R &= ~0x7F00;    // 1) disable timer1A during setup
  TIMER1_CFG_R = TIMER_CFG_16_BIT;    // 2) configure for 32-bit mode
  TIMER1_TBMR_R = 0x00000002;   // 3) configure for periodic mode, default down-count settings
  TIMER1_TBILR_R = period-1;    // 4) reload value
  TIMER1_TBPR_R = 0;            // 5) bus clock resolution
  TIMER1_ICR_R |= 0x00000100;    // 6) clear timer1A timeout flag
  TIMER1_IMR_R |= 0x00000100;    // 7) arm timeout interrupt
  NVIC_PRI5_R = (NVIC_PRI5_R&0xFF00FFFF)|0x00400000; // 8) priority 4
// interrupts enabled in the main program after all devices initialized
// vector number 39, interrupt number 22
  NVIC_EN0_R = 1<<22;           // 9) enable IRQ 22 in NVIC
  TIMER1_CTL_R |= 0x00000100;    // 10) enable timer1A
  EndCritical(sr);
}

// ***************** Timer2A_Init ****************
// Activate Timer2 interrupts to run user task periodically
// Inputs:  task is a pointer to a user function
//          period in units (1/clockfreq)
// Outputs: none
void Timer2A_Init(void(*task)(void), unsigned long period){
  long sr = StartCritical();
  SYSCTL_RCGCTIMER_R |= 0x04;   // 0) activate timer2
  PeriodicTask2A = task;          // user function
  TIMER2_CTL_R &= ~0x7F;    // 1) disable timer2A during setup
  TIMER2_CFG_R = TIMER_CFG_16_BIT;    // 2) configure for 32-bit mode
  TIMER2_TAMR_R = 0x00000002;   // 3) configure for periodic mode, default down-count settings
  TIMER2_TAILR_R = period-1;    // 4) reload value
  TIMER2_TAPR_R = 0;            // 5) bus clock resolution
  TIMER2_ICR_R |= 0x00000001;    // 6) clear timer2A timeout flag
  TIMER2_IMR_R |= 0x00000001;    // 7) arm timeout interrupt
  NVIC_PRI5_R = (NVIC_PRI5_R&0x00FFFFFF)|0x40000000; // 8) priority 4
// interrupts enabled in the main program after all devices initialized
// vector number 39, interrupt number 23
  NVIC_EN0_R = 1<<23;           // 9) enable IRQ 23 in NVIC
  TIMER2_CTL_R |= 0x00000001;    // 10) enable timer2A
  EndCritical(sr);
}

// ***************** Timer2B_Init ****************
// Activate Timer2 interrupts to run user task periodically
// Inputs:  task is a pointer to a user function
//          period in units (1/clockfreq)
// Outputs: none
void Timer2B_Init(void(*task)(void), unsigned long period){
  long sr = StartCritical();
  SYSCTL_RCGCTIMER_R |= 0x04;   // 0) activate timer2
  PeriodicTask2B = task;          // user function
  TIMER2_CTL_R &= ~0x7F00;    // 1) disable timer2A during setup
  TIMER2_CFG_R = TIMER_CFG_16_BIT;    // 2) configure for 32-bit mode
  TIMER2_TBMR_R = 0x00000002;   // 3) configure for periodic mode, default down-count settings
  TIMER2_TBILR_R = period-1;    // 4) reload value
  TIMER2_TBPR_R = 0;            // 5) bus clock resolution
  TIMER2_ICR_R |= 0x00000100;    // 6) clear timer2A timeout flag
  TIMER2_IMR_R |= 0x00000100;    // 7) arm timeout interrupt
  NVIC_PRI6_R = (NVIC_PRI6_R&0xFFFFFF00)|0x00000040; // 8) priority 4
// interrupts enabled in the main program after all devices initialized
// vector number 39, interrupt number 24
  NVIC_EN0_R = 1<<24;           // 9) enable IRQ 24 in NVIC
  TIMER2_CTL_R |= 0x00000100;    // 10) enable timer2A
  EndCritical(sr);
}

// ***************** Timer3B_Init ****************
// Activate Timer3 interrupts to run user task periodically
// Inputs:  task is a pointer to a user function
//          period in units (1/clockfreq)
// Outputs: none
void Timer3B_Init(void(*task)(void), unsigned long period){
  long sr = StartCritical();
  SYSCTL_RCGCTIMER_R |= 0x08;   // 0) activate timer3
  PeriodicTask3B = task;          // user function
  TIMER3_CTL_R &= ~0x7F00;    // 1) disable timer3A during setup
  TIMER3_CFG_R = TIMER_CFG_16_BIT;    // 2) configure for 32-bit mode
  TIMER3_TBMR_R = 0x00000003;   // 3) configure for periodic mode, default down-count settings
  TIMER3_TBILR_R = period-1;    // 4) reload value
  TIMER3_TBPR_R = 0;            // 5) bus clock resolution
  TIMER3_ICR_R |= 0x00000100;    // 6) clear timer3A timeout flag
  TIMER3_IMR_R |= 0x00000100;    // 7) arm timeout interrupt
  NVIC_PRI9_R = (NVIC_PRI9_R&0xFFFFFFFF)|0x00000040; // 8) priority 4
// interrupts enabled in the main program after all devices initialized
// vector number 39, interrupt number 36
  NVIC_EN1_R = 1<<4;           // 9) enable IRQ 36 in NVIC
  TIMER3_CTL_R |= 0x00000100;    // 10) enable timer3A
  EndCritical(sr);
}

// ***************** Timer4A_Init ****************
// Activate Timer4 interrupts to run user task periodically
// Inputs:  task is a pointer to a user function
//          period in units (1/clockfreq)
// Outputs: none
void Timer4A_Init(void(*task)(void), unsigned long period){
  long sr = StartCritical();
  SYSCTL_RCGCTIMER_R |= 0x10;   // 0) activate timer4
  PeriodicTask4A = task;          // user function
  TIMER4_CTL_R &= ~0x7F;    // 1) disable timer4A during setup
  TIMER4_CFG_R = TIMER_CFG_16_BIT;    // 2) configure for 32-bit mode
  TIMER4_TAMR_R = 0x00000004;   // 3) configure for periodic mode, default down-count settings
  TIMER4_TAILR_R = period-1;    // 4) reload value
  TIMER4_TAPR_R = 0;            // 5) bus clock resolution
  TIMER4_ICR_R |= 0x00000001;    // 6) clear timer4A timeout flag
  TIMER4_IMR_R |= 0x00000001;    // 7) arm timeout interrupt
  NVIC_PRI17_R = (NVIC_PRI17_R&0xFF00FFFF)|0x00400000; // 8) priority 4
// interrupts enabled in the main program after all devices initialized
// vector number 39, interrupt number 70
  NVIC_EN2_R = 1<<6;           // 9) enable IRQ 70 in NVIC
  TIMER4_CTL_R |= 0x00000001;    // 10) enable timer4A
  EndCritical(sr);
}

// ***************** Timer4B_Init ****************
// Activate Timer4 interrupts to run user task periodically
// Inputs:  task is a pointer to a user function
//          period in units (1/clockfreq)
// Outputs: none
void Timer4B_Init(void(*task)(void), unsigned long period){
  long sr = StartCritical();
  SYSCTL_RCGCTIMER_R |= 0x10;   // 0) activate timer4
  PeriodicTask4B = task;          // user function
  TIMER4_CTL_R &= ~0x7F00;    // 1) disable timer4A during setup
  TIMER4_CFG_R = TIMER_CFG_16_BIT;    // 2) configure for 32-bit mode
  TIMER4_TBMR_R = 0x00000004;   // 3) configure for periodic mode, default down-count settings
  TIMER4_TBILR_R = period-1;    // 4) reload value
  TIMER4_TBPR_R = 0;            // 5) bus clock resolution
  TIMER4_ICR_R |= 0x00000100;    // 6) clear timer4A timeout flag
  TIMER4_IMR_R |= 0x00000100;    // 7) arm timeout interrupt
  NVIC_PRI17_R = (NVIC_PRI17_R&0x00FFFFFF)|0x40000000; // 8) priority 4
// interrupts enabled in the main program after all devices initialized
// vector number 39, interrupt number 71
  NVIC_EN2_R = 1<<7;           // 9) enable IRQ 71 in NVIC
  TIMER4_CTL_R |= 0x00000100;    // 10) enable timer4A
  EndCritical(sr);
}

// ***************** Timer5A_Init ****************
// Activate Timer5 interrupts to run user task periodically
// Inputs:  task is a pointer to a user function
//          period in units (1/clockfreq)
// Outputs: none
void Timer5A_Init(void(*task)(void), unsigned long period){
  long sr = StartCritical();
  SYSCTL_RCGCTIMER_R |= 0x20;   // 0) activate timer5
  PeriodicTask5A = task;          // user function
  TIMER5_CTL_R &= ~0x7F;    // 1) disable timer5A during setup
  TIMER5_CFG_R = TIMER_CFG_16_BIT;    // 2) configure for 32-bit mode
  TIMER5_TAMR_R = 0x00000005;   // 3) configure for periodic mode, default down-count settings
  TIMER5_TAILR_R = period-1;    // 4) reload value
  TIMER5_TAPR_R = 0;            // 5) bus clock resolution
  TIMER5_ICR_R |= 0x00000001;    // 6) clear timer5A timeout flag
  TIMER5_IMR_R |= 0x00000001;    // 7) arm timeout interrupt
  NVIC_PRI23_R = (NVIC_PRI23_R&0xFFFFFF00)|0x00000040; // 8) priority 4
// interrupts enabled in the main program after all devices initialized
// vector number 39, interrupt number 92
  NVIC_EN2_R = 1<<28;           // 9) enable IRQ 92 in NVIC
  TIMER5_CTL_R |= 0x00000001;    // 10) enable timer5A
  EndCritical(sr);
}

// ***************** Timer5B_Init ****************
// Activate Timer5 interrupts to run user task periodically
// Inputs:  task is a pointer to a user function
//          period in units (1/clockfreq)
// Outputs: none
void Timer5B_Init(void(*task)(void), unsigned long period){
  long sr = StartCritical();
  SYSCTL_RCGCTIMER_R |= 0x20;   // 0) activate timer5
  PeriodicTask5B = task;          // user function
  TIMER5_CTL_R &= ~0x7F00;    // 1) disable timer5A during setup
  TIMER5_CFG_R = TIMER_CFG_16_BIT;    // 2) configure for 32-bit mode
  TIMER5_TBMR_R = 0x00000005;   // 3) configure for periodic mode, default down-count settings
  TIMER5_TBILR_R = period-1;    // 4) reload value
  TIMER5_TBPR_R = 0;            // 5) bus clock resolution
  TIMER5_ICR_R |= 0x00000100;    // 6) clear timer5A timeout flag
  TIMER5_IMR_R |= 0x00000100;    // 7) arm timeout interrupt
  NVIC_PRI23_R = (NVIC_PRI23_R&0xFFFF00FF)|0x00004000; // 8) priority 4
// interrupts enabled in the main program after all devices initialized
// vector number 39, interrupt number 93
  NVIC_EN2_R = 1<<29;           // 9) enable IRQ 93 in NVIC
  TIMER5_CTL_R |= 0x00000100;    // 10) enable timer5A
  EndCritical(sr);
}








void (*ADC_hook)(uint16_t);
// This initialization function sets up the ADC according to the
// following parameters.  Any parameters not explicitly listed
// below are not modified:
// Timer0A: enabled
// Mode: 32-bit, down counting
// One-shot or periodic: periodic
// Interval value: programmable using 32-bit period
// Sample time is busPeriod*period
// Max sample rate: <=125,000 samples/second
// Sequencer 0 priority: 1st (highest)
// Sequencer 1 priority: 2nd
// Sequencer 2 priority: 3rd
// Sequencer 3 priority: 4th (lowest)
// SS3 triggering event: Timer0A
// SS3 1st sample source: programmable using variable 'channelNum' [0:11]
// SS3 interrupts: enabled and promoted to controller
void ADC0_InitTimer0ATriggerSeq3(uint8_t channelNum, uint32_t period, void(*function)(uint16_t)){
  long sr = StartCritical();
  ADC_hook = function;
  volatile uint32_t delay;
  // **** GPIO pin initialization ****
  switch(channelNum){             // 1) activate clock
    case 0:
    case 1:
    case 2:
    case 3:
    case 8:
    case 9:                       //    these are on GPIO_PORTE
      SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R4; break;
    case 4:
    case 5:
    case 6:
    case 7:                       //    these are on GPIO_PORTD
      SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R3; break;
    case 10:
    case 11:                      //    these are on GPIO_PORTB
      SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R1; break;
    default: return;              //    0 to 11 are valid channels on the LM4F120
  }
  delay = SYSCTL_RCGCGPIO_R;      // 2) allow time for clock to stabilize
  delay = SYSCTL_RCGCGPIO_R;
  switch(channelNum){
    case 0:                       //      Ain0 is on PE3
      GPIO_PORTE_DIR_R &= ~0x08;  // 3.0) make PE3 input
      GPIO_PORTE_AFSEL_R |= 0x08; // 4.0) enable alternate function on PE3
      GPIO_PORTE_DEN_R &= ~0x08;  // 5.0) disable digital I/O on PE3
      GPIO_PORTE_AMSEL_R |= 0x08; // 6.0) enable analog functionality on PE3
      break;
    case 1:                       //      Ain1 is on PE2
      GPIO_PORTE_DIR_R &= ~0x04;  // 3.1) make PE2 input
      GPIO_PORTE_AFSEL_R |= 0x04; // 4.1) enable alternate function on PE2
      GPIO_PORTE_DEN_R &= ~0x04;  // 5.1) disable digital I/O on PE2
      GPIO_PORTE_AMSEL_R |= 0x04; // 6.1) enable analog functionality on PE2
      break;
    case 2:                       //      Ain2 is on PE1
      GPIO_PORTE_DIR_R &= ~0x02;  // 3.2) make PE1 input
      GPIO_PORTE_AFSEL_R |= 0x02; // 4.2) enable alternate function on PE1
      GPIO_PORTE_DEN_R &= ~0x02;  // 5.2) disable digital I/O on PE1
      GPIO_PORTE_AMSEL_R |= 0x02; // 6.2) enable analog functionality on PE1
      break;
    case 3:                       //      Ain3 is on PE0
      GPIO_PORTE_DIR_R &= ~0x01;  // 3.3) make PE0 input
      GPIO_PORTE_AFSEL_R |= 0x01; // 4.3) enable alternate function on PE0
      GPIO_PORTE_DEN_R &= ~0x01;  // 5.3) disable digital I/O on PE0
      GPIO_PORTE_AMSEL_R |= 0x01; // 6.3) enable analog functionality on PE0
      break;
    case 4:                       //      Ain4 is on PD3
      GPIO_PORTD_DIR_R &= ~0x08;  // 3.4) make PD3 input
      GPIO_PORTD_AFSEL_R |= 0x08; // 4.4) enable alternate function on PD3
      GPIO_PORTD_DEN_R &= ~0x08;  // 5.4) disable digital I/O on PD3
      GPIO_PORTD_AMSEL_R |= 0x08; // 6.4) enable analog functionality on PD3
      break;
    case 5:                       //      Ain5 is on PD2
      GPIO_PORTD_DIR_R &= ~0x04;  // 3.5) make PD2 input
      GPIO_PORTD_AFSEL_R |= 0x04; // 4.5) enable alternate function on PD2
      GPIO_PORTD_DEN_R &= ~0x04;  // 5.5) disable digital I/O on PD2
      GPIO_PORTD_AMSEL_R |= 0x04; // 6.5) enable analog functionality on PD2
      break;
    case 6:                       //      Ain6 is on PD1
      GPIO_PORTD_DIR_R &= ~0x02;  // 3.6) make PD1 input
      GPIO_PORTD_AFSEL_R |= 0x02; // 4.6) enable alternate function on PD1
      GPIO_PORTD_DEN_R &= ~0x02;  // 5.6) disable digital I/O on PD1
      GPIO_PORTD_AMSEL_R |= 0x02; // 6.6) enable analog functionality on PD1
      break;
    case 7:                       //      Ain7 is on PD0
      GPIO_PORTD_DIR_R &= ~0x01;  // 3.7) make PD0 input
      GPIO_PORTD_AFSEL_R |= 0x01; // 4.7) enable alternate function on PD0
      GPIO_PORTD_DEN_R &= ~0x01;  // 5.7) disable digital I/O on PD0
      GPIO_PORTD_AMSEL_R |= 0x01; // 6.7) enable analog functionality on PD0
      break;
    case 8:                       //      Ain8 is on PE5
      GPIO_PORTE_DIR_R &= ~0x20;  // 3.8) make PE5 input
      GPIO_PORTE_AFSEL_R |= 0x20; // 4.8) enable alternate function on PE5
      GPIO_PORTE_DEN_R &= ~0x20;  // 5.8) disable digital I/O on PE5
      GPIO_PORTE_AMSEL_R |= 0x20; // 6.8) enable analog functionality on PE5
      break;
    case 9:                       //      Ain9 is on PE4
      GPIO_PORTE_DIR_R &= ~0x10;  // 3.9) make PE4 input
      GPIO_PORTE_AFSEL_R |= 0x10; // 4.9) enable alternate function on PE4
      GPIO_PORTE_DEN_R &= ~0x10;  // 5.9) disable digital I/O on PE4
      GPIO_PORTE_AMSEL_R |= 0x10; // 6.9) enable analog functionality on PE4
      break;
    case 10:                      //       Ain10 is on PB4
      GPIO_PORTB_DIR_R &= ~0x10;  // 3.10) make PB4 input
      GPIO_PORTB_AFSEL_R |= 0x10; // 4.10) enable alternate function on PB4
      GPIO_PORTB_DEN_R &= ~0x10;  // 5.10) disable digital I/O on PB4
      GPIO_PORTB_AMSEL_R |= 0x10; // 6.10) enable analog functionality on PB4
      break;
    case 11:                      //       Ain11 is on PB5
      GPIO_PORTB_DIR_R &= ~0x20;  // 3.11) make PB5 input
      GPIO_PORTB_AFSEL_R |= 0x20; // 4.11) enable alternate function on PB5
      GPIO_PORTB_DEN_R &= ~0x20;  // 5.11) disable digital I/O on PB5
      GPIO_PORTB_AMSEL_R |= 0x20; // 6.11) enable analog functionality on PB5
      break;
  }
  SYSCTL_RCGCADC_R |= 0x01;     // activate ADC0 
  SYSCTL_RCGCTIMER_R |= 0x01;   // activate timer0 
  delay = SYSCTL_RCGCTIMER_R;   // allow time to finish activating
  TIMER0_CTL_R = 0x00000000;    // disable timer0A during setup
  TIMER0_CTL_R |= 0x00000020;   // enable timer0A trigger to ADC
  TIMER0_CFG_R = TIMER_CFG_32_BIT_TIMER;             // configure for 32-bit timer mode
  TIMER0_TAMR_R = 0x00000002;   // configure for periodic mode, default down-count settings
  TIMER0_TAPR_R = 0;            // prescale value for trigger
  TIMER0_TAILR_R = period-1;    // start value for trigger
  TIMER0_IMR_R = 0x00000000;    // disable all interrupts
  TIMER0_CTL_R |= 0x00000001;   // enable timer0A 32-b, periodic, no interrupts
  ADC0_PC_R = 0x01;         // configure for 125K samples/sec
  ADC0_SSPRI_R = 0x3210;    // sequencer 0 is highest, sequencer 3 is lowest
  ADC0_ACTSS_R &= ~0x08;    // disable sample sequencer 3
  ADC0_EMUX_R = (ADC0_EMUX_R&0xFFFF0FFF)+0x5000; // timer trigger event
  ADC0_SSMUX3_R = channelNum;
  ADC0_SSCTL3_R = 0x06;          // set flag and end                       
  ADC0_IM_R |= 0x08;             // enable SS3 interrupts
  ADC0_ACTSS_R |= 0x08;          // enable sample sequencer 3
  NVIC_PRI4_R = (NVIC_PRI4_R&0xFFFF00FF)|0x00004000; //priority 1
  NVIC_EN0_R = 1<<17;              // enable interrupt 17 in NVIC
  EndCritical(sr);
}

volatile uint16_t ADCvalue;
void ADC0Seq3_Handler(void){struct FIFO_PUT_FUNCTION_LISTING *tempPt; 
  ADC0_ISC_R = 0x08;          // acknowledge ADC sequence 3 completion
  #if ISR_STACK
  EnterISR();
  #endif 
  ADCvalue = ADC0_SSFIFO3_R;
  ADC_hook(ADCvalue);
  #if ISR_STACK
  ExitISR();
  #endif
}

uint16_t ADC0Seq3_Get(void){
  return ADCvalue;
}
