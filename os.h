// os.h
// Matthew DeKoning
// Andrew Lynch
// March 7, 2017
// March 7, 2017

#ifndef __OS__
#define __OS__

#include <stdint.h>

#define ON 1
#define OFF 0
#define OS_DEBUG OFF
#define LCD_OPTIMIZATION OFF
#define OS_AGING ON
#define OS_PRI_INVERSION ON

/* ---------------------------------------------------- */
/*                       OS DEBUG                       */
/* ---------------------------------------------------- */

#if OS_DEBUG
#define PB2  (*((volatile unsigned long *)0x40005010))
#define PB3  (*((volatile unsigned long *)0x40005020))
#define PB4  (*((volatile unsigned long *)0x40005040))
#define PB5  (*((volatile unsigned long *)0x40005080))
#define PB6  (*((volatile unsigned long *)0x40005100))
#define Debug_Task0         \
      PB3 = 0x00;            \
      PB4 = 0x00;            \
      PB5 = 0x00;            \
      PB6 = 0x00;            \
      PB2 ^= 0x04;
#define Debug_Task1                  \
      PB3 = 0x08;            \
      PB4 = 0x00;            \
      PB5 = 0x00;            \
      PB6 = 0x00;            \
      PB2 ^= 0x04;
#define Debug_Task2                  \
      PB3 = 0x00;            \
      PB4 = 0x10;            \
      PB5 = 0x00;            \
      PB6 = 0x00;            \
      PB2 ^= 0x04;
#define Debug_Task3                  \
      PB3 = 0x08;            \
      PB4 = 0x10;            \
      PB5 = 0x00;            \
      PB6 = 0x00;            \
      PB2 ^= 0x04;
#define Debug_Task4                  \
      PB3 = 0x00;            \
      PB4 = 0x00;            \
      PB5 = 0x20;            \
      PB6 = 0x00;            \
      PB2 ^= 0x04;
#define Debug_Task5                  \
      PB3 = 0x08;            \
      PB4 = 0x00;            \
      PB5 = 0x20;            \
      PB6 = 0x00;            \
      PB2 ^= 0x04;
#define Debug_Task6                  \
      PB3 = 0x00;            \
      PB4 = 0x10;            \
      PB5 = 0x20;            \
      PB6 = 0x00;            \
      PB2 ^= 0x04;
#define Debug_Task7                  \
      PB3 = 0x08;            \
      PB4 = 0x10;            \
      PB5 = 0x20;            \
      PB6 = 0x00;            \
      PB2 ^= 0x04;
#define Debug_Task8                  \
      PB3 = 0x00;            \
      PB4 = 0x00;            \
      PB5 = 0x00;            \
      PB6 = 0x40;            \
      PB2 ^= 0x04;
#define Debug_Task9                  \
      PB3 = 0x08;            \
      PB4 = 0x00;            \
      PB5 = 0x00;            \
      PB6 = 0x40;            \
      PB2 ^= 0x04;
#define Debug_Task10                 \
      PB3 = 0x00;            \
      PB4 = 0x10;            \
      PB5 = 0x00;            \
      PB6 = 0x40;            \
      PB2 ^= 0x04;
#define Debug_Task11                 \
      PB3 = 0x08;            \
      PB4 = 0x10;            \
      PB5 = 0x00;            \
      PB6 = 0x40;            \
      PB2 ^= 0x04;
#define Debug_Task12                 \
      PB3 = 0x00;            \
      PB4 = 0x00;            \
      PB5 = 0x20;            \
      PB6 = 0x40;            \
      PB2 ^= 0x04;
#define Debug_Task13                 \
      PB3 = 0x08;            \
      PB4 = 0x00;            \
      PB5 = 0x20;            \
      PB6 = 0x40;            \
      PB2 ^= 0x04;
#define Debug_Task14                 \
      PB3 = 0x00;            \
      PB4 = 0x10;            \
      PB5 = 0x20;            \
      PB6 = 0x40;            \
      PB2 ^= 0x04;
#define Debug_Task15                 \
      PB3 = 0x08;            \
      PB4 = 0x10;            \
      PB5 = 0x20;            \
      PB6 = 0x40;            \
      PB2 ^= 0x04;
#endif //DEBUG


/* ---------------------------------------------------- */
/*                     OS TIMESLICES                    */
/* ---------------------------------------------------- */

#define TIME_1MS    80000          
#define TIME_2MS    (2*TIME_1MS)  
#define TIME_500US  (TIME_1MS/2)  
#define TIME_250US  (TIME_1MS/5) 

/* ---------------------------------------------------- */
/*                     OS TIMESLICES                    */
/* ---------------------------------------------------- */

// Resolution: 0.1ms
#define PERIODIC_TIME_500US    5
#define PERIODIC_TIME_1MS      10
#define PERIODIC_FREQ_10kHz    PERIODIC_TIME_1MS/10
#define PeriodicPeriod(Period_ms) (10 * Period_ms)
#define PeriodicFrequency(Freq_kHz) (PERIODIC_FREQ_10kHz * Freq_kHz)/10

/* ---------------------------------------------------- */
/*                      OS OBJECTS                      */
/* ---------------------------------------------------- */

struct TCB{
	//stack
	uint32_t *stackPt;
	uint32_t MoreStack[83];
	uint32_t Regs[14];
	void (*PC)(void);
	uint32_t PSR;
	//state variables
	char* name;
	uint32_t sleep;
  uint32_t refillTimeSlice;
  uint32_t remainingTimeSlice;
  uint16_t numCompletedTimeSlices;
  uint32_t age;
  uint8_t PriInversionInc;
  //periodic TCBs
  uint8_t Periodic_period;
  uint8_t Periodic_countdown;
  uint8_t id;
};

struct TCBPointer{
  struct TCB* tcb;
};
extern struct TCBPointer TCBPointerArray[33];
extern struct TCB *TCBReservoir;

struct semaphore{
  int32_t value;
  uint32_t reg;
  uint8_t CurrentTCB_Index;
};
extern struct semaphore SPI;
struct LCDMailboxMsg{
  uint8_t device;
  uint8_t line;
  char message[20];
};

struct UARTMailboxMsg{
  char *message;
};

enum{
  SW1,
  SW2
};

enum{
  Push,
  Release
};

/* ---------------------------------------------------- */
/*                      OS GLOBALS                      */
/* ---------------------------------------------------- */

extern uint32_t OSInterruptTask_R;
extern int32_t TCB_Index;
extern uint32_t first100Times[100];
extern uint32_t first100Ms[100];
extern uint32_t firstTypes[100];
extern uint8_t count;

/* ---------------------------------------------------- */
/*                      OS PROTOTYPES                   */
/* ---------------------------------------------------- */

uint32_t * getMS();
uint32_t * getTimes();
uint32_t * getTypes();
void calcJitter(int i);


// ******** OS_Init ************
// initialize operating system, disable interrupts until OS_Launch
// initialize OS controlled I/O: serial, ADC, systick, LaunchPad I/O and timers 
// Inputs: number of 12.5ns clock cycles for each time slice
//         you may select the units of this parameter
// output: none
void OS_EarlyInit();
void OS_Init(uint32_t timeSlice); 

// ******** OS_Launch ************
// Launches the OS. Starts interrupts, calls scheduler, pends PendSV
// input:  none
// output: none
void OS_Launch(void);

// ******** OS_ResetCurrentTimeSlice ************
// Resets the current TCBs Remaining Time Slice
// For use by systick
// input:  none
// output: none
void OS_ResetCurrentTimeSlice(void);


// ******** OS_InitSemaphore ************
// initialize semaphore 
// input:  pointer to a semaphore
// output: none
void OS_InitSemaphore(struct semaphore* semaPt, int32_t value); 

// ******** OS_Wait ************
// decrement semaphore 
// Lab2 spinlock
// Lab3 block if less than zero
// input:  pointer to a counting semaphore
// output: none
void OS_Wait(struct semaphore* semaPt); 

// ******** OS_Signal ************
// increment semaphore 
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a counting semaphore
// output: none
void OS_Signal(struct semaphore* semaPt); 

// ******** OS_Signal_NoSuspend ************
// increment semaphore 
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a counting semaphore
// output: none
void OS_Signal_NoSuspend(struct semaphore* semaPt); 

// ******** OS_bWait ************
// Lab2 spinlock, set to 0
// Lab3 block if less than zero
// input:  pointer to a binary semaphore
// output: none
void OS_bWait(struct semaphore* semaPt); 

// ******** OS_bSignal ************
// Lab2 spinlock, set to 1
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a binary semaphore
// output: none
void OS_bSignal(struct semaphore* semaPt); 

//******** OS_AddThread *************** 
// add a foregound thread to the scheduler
// Inputs: pointer to a void/void foreground task
//         number of bytes allocated for its stack
//         priority, 0 is highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// stack size must be divisable by 8 (aligned to double word boundary)
// In Lab 2, you can ignore both the stackSize and priority fields
// In Lab 3, you can ignore the stackSize fields
uint8_t OS_AddThread(void(*task)(void), int32_t priority, char* name, uint8_t id);

//******** OS_GetMaxJitter *************** 
// returns the thread ID for the currently running thread
// Inputs: none
// Outputs: Thread ID, number greater than zero 
long OS_GetMaxJitter(void);

//******** OS_AddSwitchThread *************** 
// add a background task to run whenever the SW2 (PF0) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is highest, 5 is lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed user task will run to completion and return
// This task can not spin block loop sleep or kill
// This task can call issue OS_Signal, it can call OS_AddThread
// This task does not have a Thread ID
// In lab 2, this function can be ignored
// In lab 3, this command will be called will be called 0 or 1 times
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
uint8_t OS_AddSwitchThread(int switchNum, int pushOrRelease, void (*task)(void), char* name);

void OS_ActivateSwitch(int switchNum, int pushOrRelease);

// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// You are free to select the time resolution for this function
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(unsigned long sleepTime); 

// ******** OS_Kill ************
// kill the currently running thread, release its TCB and stack
// input:  none
// output: none
void OS_Kill(void); 

// ******** OS_Suspend ************
// suspend execution of currently running thread
// scheduler will choose another thread to execute
// Can be used to implement cooperative multitasking 
// Same function as OS_Sleep(0)
// input:  none
// output: none
void OS_Suspend(void);

// ******** OS_Fifo_Init ************
// Initialize the Fifo to be empty
// Inputs: size
// Outputs: none 
// In Lab 2, you can ignore the size field
// In Lab 3, you should implement the user-defined fifo size
// In Lab 3, you can put whatever restrictions you want on size
//    e.g., 4 to 64 elements
//    e.g., must be a power of 2,4,8,16,32,64,128
void OS_Fifo_Init(void);

// ******** OS_Fifo_PutFast ************
// Enter one data sample into the Fifo
// Called from the background, so no waiting 
// Inputs:  data
// Outputs: true if data is properly saved,
//          false if data not saved, because it was full
int OS_Fifo_PutFast(unsigned long data);  

// ******** OS_Fifo_Put ************
// Enter one data sample into the Fifo
// Uses waiting
// Cannot be called in background 
// Inputs:  data
// Outputs: none
void OS_Fifo_Put(unsigned long data);  

// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data 
unsigned long OS_Fifo_Get(void);

// ******** OS_Fifo_Size ************
// Check the status of the Fifo
// Inputs: none
// Outputs: returns the number of elements in the Fifo
//          greater than zero if a call to OS_Fifo_Get will return right away
//          zero or less than zero if the Fifo is empty 
//          zero or less than zero if a call to OS_Fifo_Get will spin or block
unsigned long OS_Fifo_Size(void);

// ******** LCD_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: none
void LCD_MailBox_Init(void);

// ******** LCD_MailBox_Count ************
// Returns amount of items in mailbox
// Inputs:  none
// Outputs: number of items in mailbox
int LCD_MailBox_Count(void);

// ******** LCD_MailBox_Send ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: none
// This function will be called from a foreground thread
// It will spin/block if the MailBox contains data not yet received 
void LCD_MailBox_Send(int device, int line, char* message);

// ******** LCD_MailBox_Recv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty 
struct LCDMailboxMsg  LCD_MailBox_Recv(void);

void OS_EnableLCD(void);

void OS_DisableLCD(void);

void OS_EnableInterpreter(void);

void OS_DisableInterpreter(void);


// ******** UART_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: none
void UART_MailBox_Init(void);

// ******** UART_MailBox_Count ************
// Returns amount of items in mailbox
// Inputs:  none
// Outputs: number of items in mailbox
int UART_MailBox_Count(void);

// ******** UART_MailBox_InterpreterSend ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: none
// This function will be called from a foreground thread
// It will spin/block if the MailBox contains data not yet received 
void UART_MailBox_InterpreterSend(char* message);

// ******** UART_MailBox_ClientSendFast ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: true if data is properly saved,
//          false if data not saved, because it was full
uint8_t UART_MailBox_ClientSendFast(char* message);

// ******** UART_MailBox_ClientSend ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: none
// This function will be called from a foreground thread
// It will spin/block if the MailBox contains data not yet received 
void UART_MailBox_ClientSend(char* message);

// ******** UART_MailBox_InterpreterRecv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty 
struct UARTMailboxMsg  UART_MailBox_InterpreterRecv(void);

// ******** UART_MailBox_ClientRecv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty 
struct UARTMailboxMsg  UART_MailBox_ClientRecv(void);

// ******** OS_Time ************
// return the system time 
// Inputs:  none
// Outputs: time in 12.5ns units, 0 to 4294967295
// The time resolution should be less than or equal to 1us, and the precision 32 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_TimeDifference have the same resolution and precision 
uint32_t OS_Time(void);

// ******** OS_TimeDifference ************
// Calculates difference between two times
// Inputs:  two times measured with OS_Time
// Outputs: time difference in 12.5ns units 
// The time resolution should be less than or equal to 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_Time have the same resolution and precision 
uint32_t OS_TimeDifference(uint32_t start, uint32_t stop);

// ******** OS_ClearMsTime ************
// sets the system time to zero (from Lab 1)
// Inputs:  none
// Outputs: none
// You are free to change how this works
void OS_ClearTime(void);

// ******** OS_MsTime ************
// reads the current time in msec (from Lab 1)
// Inputs:  none
// Outputs: time in ms units
// You are free to select the time resolution for this function
// It is ok to make the resolution to match the first call to OS_AddPeriodicThread
unsigned long OS_MsTime(void);

void OS_Delay1ms(int i);

//******** OS_Launch *************** 
// start the scheduler, enable interrupts
// Inputs: none
// Outputs: none (does not return)
// In Lab 2, you can ignore the theTimeSlice field
// In Lab 3, you should implement the user-defined TimeSlice field
// It is ok to limit the range of theTimeSlice to match the 24-bit SysTick
void OS_Launch(void);

void calcJitter(int i);
long OS_getMaxJitter(int i);
#endif //__OS__