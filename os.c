// os.c
// Matthew DeKoning
// Andrew Lynch
// March 6, 2017
// March 6, 2017

#include "os.h"
#include "FIFO.h"
#include "Switch.h"
#include "SysTick.h"
#include "interpreter.h"
#include "UART.h"
#include "ST7735.h"
#include <stdio.h>
#include "../inc/tm4c123gh6pm.h"
#include "heap.h"
#include "diskio.h"
typedef void(entry_t)(void);

void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode
uint32_t CountLeadingZeros(uint32_t);
void OS_AsmLaunch(void);

#define TRUE 1
#define FALSE 0
#define NULL 0

#define NUMBER_OF_TCBS 32
#define SIZE_OF_TCB 428
#define LCD_MAILBOX_FIFO_SIZE 32
#define UART_MAILBOX_FIFO_SIZE 32
#define FIFOSIZE 16
#define FIFOSUCCESS 1
#define FIFOFAIL 0
#define tcb(a) (*(TCBPointerArray[a].tcb))

/* ---------------------------------------------------- */
/*                  OS PRIVATE OBJECTS                  */
/* ---------------------------------------------------- */



/* ---------------------------------------------------- */
/*                      OS GLOBALS                      */
/* ---------------------------------------------------- */

struct TCBPointer TCBPointerArray[33];
struct TCB *TCBReservoir;
struct semaphore SPI;
int TCBFrequency[32];
uint32_t Priority_R;
uint32_t Sleeping_R;
uint32_t Blocking_R;
uint32_t Age_R;
uint32_t PriInversion_R;
uint32_t OSInterruptTask_R;
int32_t TCB_Index;
int32_t NextTCB_Index;
uint32_t static OS_TimeSlice;
uint32_t static OS_DisplayTimeSlice;
#define OS_RunTime TIMER3_TAV_R
uint32_t OS_100UsRunTime = 0;
#if OS_DEBUG

#endif
void (*PeriodicThread0)(void);
void (*PeriodicThread1)(void);
void (*PeriodicThread2)(void);
void (*PeriodicThread3)(void);
void (*PeriodicThread4)(void);
void (*PeriodicThread5)(void);
void (*PeriodicThread6)(void);
void (*PeriodicThread7)(void);
void (*Switch1PushThread)(void);
void (*Switch1ReleaseThread)(void);
void (*Switch2PushThread)(void);
void (*Switch2ReleaseThread)(void);


AddIndexFifo(OS, FIFOSIZE, unsigned long, FIFOSUCCESS, FIFOFAIL)
static struct semaphore OS_FIFO_Free;
static struct semaphore OS_FIFO_Valid;

AddIndexFifo(LCDMailbox, LCD_MAILBOX_FIFO_SIZE, struct LCDMailboxMsg, FIFOSUCCESS, FIFOFAIL)
static struct semaphore LCDMailbox_Free;
static struct semaphore LCDMailbox_Valid;

AddIndexFifo(UARTMailbox, UART_MAILBOX_FIFO_SIZE, struct UARTMailboxMsg, FIFOSUCCESS, FIFOFAIL)
static struct semaphore UARTMailbox_Free;
static struct semaphore UARTMailbox_InterpreterValid;
static struct semaphore UARTMailbox_ClientValid;

/* ---------------------------------------------------- */
/*                 OS PRIVATE PROTOTYPES                */
/* ---------------------------------------------------- */

void static OS_TCBReservoirInit(void);
void static OS_PeriodicWrapper(void);
uint8_t static OS_CreateSWThread(int priority, char* name);
void static OS_SwitchWrapper(void);
void static OS_Scheduler(void);
void static OS_RunClock_Init(void);
void static OS_PeriodicThreadTimer_Init(void);
void static OS_SleepingAndAgingTimer_Init(void);
void static OS_Idle(void);
uint8_t static OS_AddIdle(void);
uint8_t static OS_AddInterpreter(void);
uint8_t static OS_AddLCD(void);
void static SetRegister(uint32_t *reg, int32_t index);
void static ClearRegister(uint32_t *reg, int32_t index);
void static SetRegisterUNSAFE(uint32_t *reg, int32_t index);
void static ClearRegisterUNSAFE(uint32_t *reg, int32_t index);

/* ---------------------------------------------------- */
/*                  OS IMPLEMENTATIONS                  */
/* ---------------------------------------------------- */

void PortB_Init(void){ unsigned long volatile delay;
  SYSCTL_RCGC2_R |= 0x02;       // activate port B
  delay = SYSCTL_RCGC2_R;        
  delay = SYSCTL_RCGC2_R;         
  GPIO_PORTB_DIR_R |= 0x7C;    // make PB5-2 output heartbeats
  GPIO_PORTB_AFSEL_R &= ~0x7C;   // disable alt funct on PB5-2
  GPIO_PORTB_DEN_R |= 0x7C;     // enable digital I/O on PB5-2
  GPIO_PORTB_PCTL_R = ~0x0FFFFF00;
  GPIO_PORTB_AMSEL_R &= ~0x7C;      // disable analog functionality on PB
}


long MaxJitter[16];             // largest time jitter between interrupts in usec
#define JITTERSIZE 64
unsigned long const JitterSize=JITTERSIZE;
unsigned long JitterHistogram[16][JITTERSIZE];
unsigned static long lastTime[16];  // time at previous ADC sample
unsigned long thisTime[16];         // time at current ADC sample
uint32_t first100Times[100];
uint32_t first100Ms[100];
uint32_t firstTypes[100];
uint8_t count;

long OS_getMaxJitter(int i){
  return MaxJitter[i];
}

void calcJitter(int i){
  int dumby;
  unsigned long jitter;
  thisTime[i] = OS_Time();
      dumby = (int)(lastTime[i] - thisTime[i]);
      if(dumby < 0){
        dumby = (0xFFFFFFFF - (~dumby+1)) - tcb(i).Periodic_period-0x200;
        if(dumby <0){
          dumby = (~dumby+1);
        }
      }
      else{
        dumby = dumby - tcb(i).Periodic_period-0x200;
        if(dumby <0){
          dumby = (~dumby+1);
        }
      }
      jitter = dumby;
      jitter = jitter/8;
      if(jitter > MaxJitter[i]){
        if(count >= 100){
        MaxJitter[i] = jitter;}} // in usec
      if(jitter >= JitterSize){
        jitter = JITTERSIZE-1;
      }
      JitterHistogram[i][jitter]++; 
      lastTime[i] = thisTime[i];
}

uint32_t OS_ECTime;
uint32_t OS_LCTime;
uint32_t OS_TotalDI;
uint32_t OS_MaxDI;
uint8_t level;

void DebugInterruptTime(void){
  int currentTime = OS_Time();
  if(level == 1){
    //handle entry times
    OS_ECTime = currentTime;
  }
  else if(level == 0){
    //handle exit times
    currentTime = currentTime - OS_ECTime;
    if(currentTime < 0){
      currentTime = ~currentTime +1;
    }
    if(currentTime > OS_MaxDI){
      OS_MaxDI = currentTime;
    }
    OS_TotalDI += currentTime;
  }
}

void EnableInterruptsWrapper(void){
  level = 0;
  DebugInterruptTime();
  EnableInterrupts();
}

void DisableInterruptsWrapper(void){
  level = 1;
  DebugInterruptTime();
  DisableInterrupts();
}

long StartCriticalWrapper(){
  long sr;
  level = level + 1;
  if(level == 1){
    DebugInterruptTime();
  }
  sr = StartCritical();
  return sr;
}

void EndCriticalWrapper(long sr){
  level = level - 1;
  if(level == 0){
    DebugInterruptTime();
  }
  EndCritical(sr);
}
int OS_MaxTimeDI(){
  return OS_MaxDI;
}



uint32_t * getTimes(){
  return &first100Times[0];
}

uint32_t * getTypes(){
  return &firstTypes[0];
}
uint32_t * getMS(){
  return &first100Ms[0];
}


void OS_EnableLCD(void){
  ClearRegister(&Blocking_R,5);
}

void OS_DisableLCD(void){
  SetRegister(&Blocking_R,5);
  OS_Suspend();
}

void OS_EnableInterpreter(void){
  ClearRegister(&Blocking_R,4);
}

void OS_DisableInterpreter(void){
  SetRegister(&Blocking_R,4);
  OS_Suspend();
}

void static OS_TCBReservoirInit(void){
  struct TCB temp;
  TCB_Index = 0;
  TCBPointerArray[TCB_Index].tcb = &temp;
  TCBPointerArray[TCB_Index].tcb->id = 0xFF;
  TCBReservoir = (struct TCB *)Heap_Malloc(32*SIZE_OF_TCB);
  for(int i = 0; i < 32; i++){
    TCBPointerArray[i].tcb = &(TCBReservoir[i]);
  }
}

uint8_t static OS_AllocateTCB(uint8_t index){
  struct TCB temp;
  int32_t saved_TCB = TCB_Index;
  TCB_Index = 32;
  TCBPointerArray[TCB_Index].tcb = &temp;
  TCBPointerArray[TCB_Index].tcb->id = 0xFF;
  TCBPointerArray[index].tcb = (struct TCB *)Heap_Malloc(SIZE_OF_TCB);
  if(!TCBPointerArray[index].tcb){
    TCB_Index = saved_TCB;
    return 0;
  }
  else{
    TCB_Index = saved_TCB;
    return 1;
  }
}

void static SetRegister(uint32_t *reg, int32_t index){
#if OS_DEBUG
  long sr = StartCriticalWrapper();
#else
  long sr = StartCritical();
#endif
  uint32_t newReg = 1;
  int32_t invertIndex = 31-index;
  for(int i = 0; i < invertIndex; i++){
    newReg *= 2;
  }
  *reg |= newReg;
#if OS_DEBUG
  EndCriticalWrapper(sr);
#else
  EndCritical(sr);
#endif
}

void static ClearRegister(uint32_t *reg, int32_t index){
#if OS_DEBUG
  long sr = StartCriticalWrapper();
#else
  long sr = StartCritical();
#endif
  uint32_t newReg = 1;
  int32_t invertIndex = 31-index;
  for(int i = 0; i < invertIndex; i++){
    newReg *= 2;
  }
  *reg &= ~newReg;
#if OS_DEBUG
  EndCriticalWrapper(sr);
#else
  EndCritical(sr);
#endif
}

void static SetRegisterUNSAFE(uint32_t *reg, int32_t index){
  uint32_t newReg = 1;
  int32_t invertIndex = 31-index;
  for(int i = 0; i < invertIndex; i++){
    newReg *= 2;
  }
  *reg |= newReg;
}

void static ClearRegisterUNSAFE(uint32_t *reg, int32_t index){
  uint32_t newReg = 1;
  int32_t invertIndex = 31-index;
  for(int i = 0; i < invertIndex; i++){
    newReg *= 2;
  }
  *reg &= ~newReg;
}

void OS_ResetCurrentTimeSlice(void){
  tcb(TCB_Index).remainingTimeSlice = tcb(TCB_Index).refillTimeSlice;
  tcb(TCB_Index).numCompletedTimeSlices = tcb(TCB_Index).numCompletedTimeSlices == 0xFFFF ? 
  tcb(TCB_Index).numCompletedTimeSlices : tcb(TCB_Index).numCompletedTimeSlices + 1;
}

void OS_EarlyInit(){
  SPI.value = 1;
  PortB_Init();
  ST7735_ds_InitR(INITR_REDTAB, 4, 4, 4, 4);
  disk_initialize(0);
  
  SPI.value = 1;
}
void OS_Init(uint32_t timeSlice){
  Heap_Init();
  DisableInterrupts();
  
  //OS_TCBReservoirInit();
  TCB_Index = 0;
  NextTCB_Index = 0;
  Priority_R =     0x00000000;
  Sleeping_R =     0x00000000;
  Blocking_R =     0x00000000;
  Age_R =          0x00000000;
  PriInversion_R = 0x00000000;
  OS_TimeSlice = timeSlice;
  OS_DisplayTimeSlice = timeSlice<<2;
  /*
  for(int i = 0; i < 32; i++){
    tcb(i).stackPt = &tcb(i).Regs[0];
    for(int k = 0; k < 83; k++){
      tcb(i).MoreStack[k] = 0xE0E0E0E0;
    }
    tcb(i).Regs[0] = 0x44444444;
    tcb(i).Regs[1] = 0x55555555;
    tcb(i).Regs[2] = 0x66666666;
    tcb(i).Regs[3] = 0x77777777;
    tcb(i).Regs[4] = 0x88888888;
    tcb(i).Regs[5] = 0x99999999;
    tcb(i).Regs[6] = 0x10101010;
    tcb(i).Regs[7] = 0x11111110;
    tcb(i).Regs[8] = 0x00000000;
    tcb(i).Regs[9] = 0x11111111;
    tcb(i).Regs[10] = 0x22222222;
    tcb(i).Regs[11] = 0x33333333;
    tcb(i).Regs[12] = 0x12121212;
    tcb(i).Regs[13] = (long)&OS_Kill;
    tcb(i).PC = &OS_Suspend;
    tcb(i).PSR = 0x01000000;
    tcb(i).name = NULL;
    tcb(i).sleep = 0;
    tcb(i).remainingTimeSlice = 0;
    tcb(i).numCompletedTimeSlices = 0;
    tcb(i).age = 0;
    tcb(i).PriInversionInc = 0;
    tcb(i).Periodic_period = 0;
    tcb(i).Periodic_countdown = 0xFF;
    tcb(i).name = "Unassigned";
  }
  */
  
  SysTick_Init(OS_TimeSlice);    //for DEBUG, clocks MUST be first
  OS_RunClock_Init();
  //OS_PeriodicThreadTimer_Init();
  OS_SleepingAndAgingTimer_Init();

  OS_AddIdle();
  OS_AddInterpreter();
  OS_AddLCD();
  Switch_Init();
  UART_Init();              // initialize UART
  INTERPRETER_initArray();
  NVIC_SYS_PRI3_R |= 7<<21;
}

void OS_Launch(void){
  TCB_Index = 31;
  OS_ClearTime();
  OS_Scheduler();
  OS_AsmLaunch();
}

void OS_InitSemaphore(struct semaphore* semaPt, int32_t value){
  semaPt->value = value;
  semaPt->reg = 0x00000000;
  semaPt->CurrentTCB_Index = 0xFF;
}

void OS_Wait(struct semaphore *semaPt){
#if OS_DEBUG
  long sr = StartCriticalWrapper();
#else
  long sr = StartCritical();
#endif
  semaPt->value -= 1;
  if(semaPt->value < 0){
    SetRegisterUNSAFE(&semaPt->reg,TCB_Index);
    SetRegisterUNSAFE(&Blocking_R,TCB_Index);
    if((semaPt->CurrentTCB_Index != 0xFF)&&(semaPt->CurrentTCB_Index > TCB_Index)){ //priority inversion
      SetRegisterUNSAFE(&PriInversion_R,semaPt->CurrentTCB_Index);
      tcb(semaPt->CurrentTCB_Index).PriInversionInc = 
        (semaPt->CurrentTCB_Index - TCB_Index + 1) > tcb(semaPt->CurrentTCB_Index).PriInversionInc ?
        (semaPt->CurrentTCB_Index - TCB_Index + 1) : tcb(semaPt->CurrentTCB_Index).PriInversionInc;
    }
#if OS_DEBUG
  EndCriticalWrapper(sr);
#else
  EndCritical(sr);
#endif
    OS_Suspend();
#if OS_DEBUG
  sr = StartCriticalWrapper();
#else
  sr = StartCritical();
#endif
  }
  semaPt->CurrentTCB_Index = TCB_Index;
#if OS_DEBUG
  EndCriticalWrapper(sr);
#else
  EndCritical(sr);
#endif
}

void OS_Signal(struct semaphore *semaPt){
#if OS_DEBUG
  long sr = StartCriticalWrapper();
#else
  long sr = StartCritical();
#endif
int tempIndex;
  semaPt->value += 1;
  if(semaPt->value <= 0){
    tcb(TCB_Index).PriInversionInc = 0;
    ClearRegisterUNSAFE(&PriInversion_R, TCB_Index);
    tempIndex = CountLeadingZeros(semaPt->reg);
    semaPt->CurrentTCB_Index = tempIndex == 32 ? 0xFF : tempIndex;
    ClearRegisterUNSAFE(&semaPt->reg,tempIndex);
    ClearRegisterUNSAFE(&Blocking_R,tempIndex);
#if OS_DEBUG
  EndCriticalWrapper(sr);
#else
  EndCritical(sr);
#endif
    OS_Suspend();
    return; 
  }
#if OS_DEBUG
  EndCriticalWrapper(sr);
#else
  EndCritical(sr);
#endif
}

void OS_Signal_NoSuspend(struct semaphore *semaPt){
#if OS_DEBUG
  long sr = StartCriticalWrapper();
#else
  long sr = StartCritical();
#endif
int tempIndex;
  semaPt->value += 1;
  if(semaPt->value <= 0){
    tcb(TCB_Index).PriInversionInc = 0;
    ClearRegisterUNSAFE(&PriInversion_R, TCB_Index);
    tempIndex = CountLeadingZeros(semaPt->reg);
    semaPt->CurrentTCB_Index = tempIndex == 32 ? 0xFF : tempIndex;
    ClearRegisterUNSAFE(&semaPt->reg,tempIndex);
    ClearRegisterUNSAFE(&Blocking_R,tempIndex);
#if OS_DEBUG
  EndCriticalWrapper(sr);
#else
  EndCritical(sr);
#endif
    return; 
  }
#if OS_DEBUG
  EndCriticalWrapper(sr);
#else
  EndCritical(sr);
#endif
}

uint8_t OS_AddProcess(entry_t *entry , void* text, void* data, uint8_t priority, uint8_t number){
  
}

uint8_t OS_AddThread(void(*task)(void), int32_t priority, char* name, uint8_t id){
#if OS_DEBUG
  long sr = StartCriticalWrapper();
#else
  long sr = StartCritical();
#endif
  if(!((priority<26)&&(priority>=0))){
    return 0;
  }
  int32_t index = (6+priority);
  if(!OS_AllocateTCB(index)){
    return 0;
  }
  tcb(index).name = name;
  tcb(index).stackPt = &tcb(index).Regs[0]; //set stack pointer
  tcb(index).PC = task;
  tcb(index).PSR = 0x01000000;
  tcb(index).sleep = 0;
  tcb(index).age = 0;
  tcb(index).refillTimeSlice = OS_TimeSlice;
  tcb(index).remainingTimeSlice = OS_TimeSlice;
  tcb(index).PriInversionInc = 0;
  tcb(index).Periodic_countdown = 0;
  tcb(index).Periodic_period = 0;
  tcb(index).id = id;
  SetRegisterUNSAFE(&Priority_R,index); ClearRegisterUNSAFE(&Blocking_R,index); ClearRegisterUNSAFE(&Sleeping_R,index); 
#if OS_DEBUG
  EndCriticalWrapper(sr);
#else
  EndCritical(sr);
#endif
  return 1;
}

uint8_t OS_AddSwitchThread(int switchNum, int pushOrRelease, void (*task)(void), char* name){
  int index;
  if((switchNum==SW1) && (pushOrRelease==Push)){
    index = 0;
    Switch1PushThread = task;
    Switch_AddSwitchTask(SW1, Push, OS_SwitchWrapper);
  }
  if((switchNum==SW1) && (pushOrRelease==Release)){
    index = 1;
    Switch1ReleaseThread = task;
    Switch_AddSwitchTask(SW1, Release, OS_SwitchWrapper);
  }
  if((switchNum==SW2) && (pushOrRelease==Push)){
    index = 2;
    Switch2PushThread = task;
    Switch_AddSwitchTask(SW2, Push, OS_SwitchWrapper);
  }
  if((switchNum==SW2) && (pushOrRelease==Release)){
    index = 3;
    Switch2ReleaseThread = task;
    Switch_AddSwitchTask(SW2, Release, OS_SwitchWrapper);
  }
  return OS_CreateSWThread(index, name);
}

uint8_t static OS_CreateSWThread(int priority, char* name){
#if OS_DEBUG
  long sr = StartCriticalWrapper();
#else
  long sr = StartCritical();
#endif
  if(!OS_AllocateTCB(priority)){
    return 0;
  }
  tcb(priority).name = name;
  tcb(priority).stackPt = &tcb(priority).Regs[0]; //set stack pointer
  tcb(priority).PC = OS_SwitchWrapper;
  tcb(priority).PSR = 0x01000000;
  tcb(priority).sleep = 0;
  tcb(priority).age = 0;
  tcb(priority).remainingTimeSlice = OS_TimeSlice;
  tcb(priority).PriInversionInc = 0;
  tcb(priority).Periodic_period = 0;
  tcb(priority).Periodic_countdown = 0;
  SetRegisterUNSAFE(&Priority_R,priority); ClearRegisterUNSAFE(&Blocking_R,priority); ClearRegisterUNSAFE(&Sleeping_R,priority); 
  SetRegisterUNSAFE(&Blocking_R,priority);
#if OS_DEBUG
  EndCriticalWrapper(sr);
#else
  EndCritical(sr);
#endif
  return 1;
}

void OS_ActivateSwitch(int switchNum, int pushOrRelease){
  if((switchNum==SW1) && (pushOrRelease==Push)){
    ClearRegister(&Blocking_R,0);
  }
  if((switchNum==SW1) && (pushOrRelease==Release)){
    ClearRegister(&Blocking_R,1);
  }
  if((switchNum==SW2) && (pushOrRelease==Push)){
    ClearRegister(&Blocking_R,2);
  }
  if((switchNum==SW2) && (pushOrRelease==Release)){
    ClearRegister(&Blocking_R,3);
  }
}

void static OS_SwitchWrapper(void){
  while(1){
    switch(TCB_Index){
      case 0:
        (*Switch1PushThread)();
        break;
      case 1:
        (*Switch1ReleaseThread)();
        break;
      case 2:
        (*Switch2PushThread)();
        break;
      case 3:
        (*Switch2ReleaseThread)();
        break;
    }
#if OS_DEBUG
  long sr = StartCriticalWrapper();
#else
  long sr = StartCritical();
#endif
    SetRegisterUNSAFE(&Blocking_R,TCB_Index);
#if OS_DEBUG
  EndCriticalWrapper(sr);
#else
  EndCritical(sr);
#endif
    OS_Suspend();
  }
}

void OS_Sleep(unsigned long sleepTime){
  tcb(TCB_Index).sleep = sleepTime*10;
  SetRegister(&Sleeping_R,TCB_Index);
  OS_Suspend();
}

void OS_Kill(void){
#if OS_DEBUG
  long sr = StartCriticalWrapper();
#else
  long sr = StartCritical();
#endif
  ClearRegisterUNSAFE(&Priority_R,TCB_Index);
  Heap_Free(TCBPointerArray[TCB_Index].tcb);
  TCBPointerArray[TCB_Index].tcb = 0;
#if OS_DEBUG
  EndCriticalWrapper(sr);
#else
  EndCritical(sr);
#endif
  OS_Suspend();
}

void OS_Suspend(void){
#if OS_DEBUG
  DisableInterruptsWrapper();
#else
  DisableInterrupts();
#endif
  SysTick_Disable();
  OS_Scheduler();
  NVIC_INT_CTRL_R = 0x10000000; // Trigger PendSV
#if OS_DEBUG
  if(count < 100){
    first100Times[count] = OS_Time();
    first100Ms[count] = OS_MsTime();
    firstTypes[count] = 3;
    count++;
  }  
  EnableInterruptsWrapper();
#else
  EnableInterrupts();
#endif
}

void OS_Fifo_Init(void){
  OSFifo_Init();
  OS_InitSemaphore(&OS_FIFO_Free,FIFOSIZE);
  OS_InitSemaphore(&OS_FIFO_Valid,0);
}

int OS_Fifo_PutFast(unsigned long data){
#if OS_DEBUG
  DisableInterruptsWrapper();
#else
  DisableInterrupts();
#endif
  if(OS_FIFO_Free.value > 0){
    OS_Fifo_Put(data);
    return FIFOSUCCESS;
  }
#if OS_DEBUG
  EnableInterruptsWrapper();
#else
  EnableInterrupts();
#endif
  return FIFOFAIL;
}

void OS_Fifo_Put(unsigned long data){
  OS_Wait(&OS_FIFO_Free);
#if OS_DEBUG
  DisableInterruptsWrapper();
#else
  DisableInterrupts();
#endif
  OSFifo_Put(data);
#if OS_DEBUG
  EnableInterruptsWrapper();
#else
  EnableInterrupts();
#endif
  OS_Signal(&OS_FIFO_Valid);
}

unsigned long OS_Fifo_Get(void){
  unsigned long ret;
  OS_Wait(&OS_FIFO_Valid);
#if OS_DEBUG
  DisableInterruptsWrapper();
#else
  DisableInterrupts();
#endif
  OSFifo_Get(&ret);
#if OS_DEBUG
  EnableInterruptsWrapper();
#else
  EnableInterrupts();
#endif
  OS_Signal(&OS_FIFO_Free);
  return ret;
}

unsigned long OS_Fifo_Size(void){
  return OS_FIFO_Free.value;
}

void LCD_MailBox_Init(){
  LCDMailboxFifo_Init();
  OS_InitSemaphore(&LCDMailbox_Free, LCD_MAILBOX_FIFO_SIZE);
  OS_InitSemaphore(&LCDMailbox_Valid, 0);
}

int LCD_MailBox_Count(){
  return LCDMailbox_Valid.value;
}

void LCD_MailBox_Send(int device, int line, char* message){
  int i;
  struct LCDMailboxMsg NextMail;
  NextMail.device = device;
  NextMail.line = line;
  for(i = 0; i < 20; i++){
      NextMail.message[i] = message[i];
  }
  
  OS_Wait(&LCDMailbox_Free);
#if OS_DEBUG
  DisableInterruptsWrapper();
#else
  DisableInterrupts();
#endif
  LCDMailboxFifo_Put(NextMail);
#if OS_DEBUG
  EnableInterruptsWrapper();
#else
  EnableInterrupts();
#endif
  OS_EnableLCD();
  OS_Signal(&LCDMailbox_Valid);
}

struct LCDMailboxMsg LCD_MailBox_Recv(void){
  struct LCDMailboxMsg ret;
  OS_Wait(&LCDMailbox_Valid);
#if OS_DEBUG
  DisableInterruptsWrapper();
#else
  DisableInterrupts();
#endif
  LCDMailboxFifo_Get(&ret);
#if OS_DEBUG
  EnableInterruptsWrapper();
#else
  EnableInterrupts();
#endif
  OS_Signal(&LCDMailbox_Free);
  return ret;
}

void UART_MailBox_Init(){
  UARTMailboxFifo_Init();
  OS_InitSemaphore(&UARTMailbox_Free, UART_MAILBOX_FIFO_SIZE);
  OS_InitSemaphore(&UARTMailbox_InterpreterValid, 0);
  OS_InitSemaphore(&UARTMailbox_ClientValid, 0);
}

int UART_MailBox_Count(){
  return UARTMailbox_InterpreterValid.value > UARTMailbox_ClientValid.value ? 
    UARTMailbox_InterpreterValid.value : UARTMailbox_ClientValid.value;
}

void UART_MailBox_InterpreterSend(char* message){
  int i;
  struct UARTMailboxMsg NextMail;
  for(i = 0; i < 20; i++){
      NextMail.message[i] = message[i];
  }
  OS_Wait(&UARTMailbox_Free);
#if OS_DEBUG
  DisableInterruptsWrapper();
#else
  DisableInterrupts();
#endif
  UARTMailboxFifo_Put(NextMail);
#if OS_DEBUG
  EnableInterruptsWrapper();
#else
  EnableInterrupts();
#endif
  OS_Signal(&UARTMailbox_ClientValid);
}

uint8_t UART_MailBox_ClientSendFast(char* message){
#if OS_DEBUG
  DisableInterruptsWrapper();
#else
  DisableInterrupts();
#endif
  if(UARTMailbox_Free.value > 0){
    UART_MailBox_ClientSend(message);
    return FIFOSUCCESS;
  }
#if OS_DEBUG
  EnableInterruptsWrapper();
#else
  EnableInterrupts();
#endif
  return FIFOFAIL;
}

void UART_MailBox_ClientSend(char* message){
  int i;
  struct UARTMailboxMsg NextMail;
  for(i = 0; i < 20; i++){
      NextMail.message[i] = message[i];
  }
  OS_Wait(&UARTMailbox_Free);
#if OS_DEBUG
  DisableInterruptsWrapper();
#else
  DisableInterrupts();
#endif
  UARTMailboxFifo_Put(NextMail);
#if OS_DEBUG
  EnableInterruptsWrapper();
#else
  EnableInterrupts();
#endif
  OS_Signal(&UARTMailbox_InterpreterValid);
}

struct UARTMailboxMsg UART_MailBox_InterpreterRecv(void){
  struct UARTMailboxMsg ret;
  OS_Wait(&UARTMailbox_InterpreterValid);
#if OS_DEBUG
  DisableInterruptsWrapper();
#else
  DisableInterrupts();
#endif
  UARTMailboxFifo_Get(&ret);
#if OS_DEBUG
  EnableInterruptsWrapper();
#else
  EnableInterrupts();
#endif
  OS_Signal(&UARTMailbox_Free);
  return ret;
}

struct UARTMailboxMsg UART_MailBox_ClientRecv(void){
  struct UARTMailboxMsg ret;
  OS_Wait(&UARTMailbox_ClientValid);
#if OS_DEBUG
  DisableInterruptsWrapper();
#else
  DisableInterrupts();
#endif
  UARTMailboxFifo_Get(&ret);
#if OS_DEBUG
  EnableInterruptsWrapper();
#else
  EnableInterrupts();
#endif
  OS_Signal(&UARTMailbox_Free);
  return ret;
}

void static OS_Scheduler(void){int32_t priInversionIndex, ageIndex, tempIndex, bestIndex;
#if OS_DEBUG
  long sr = StartCriticalWrapper();
#else
  long sr = StartCritical();
#endif
  uint32_t localAge_R = Age_R;
  uint32_t localPriInversion_R = PriInversion_R;
  tcb(TCB_Index).remainingTimeSlice = SysTick_GetTime();
  tcb(TCB_Index).age = 0;
  ClearRegisterUNSAFE(&Age_R,TCB_Index);
  NextTCB_Index = CountLeadingZeros((Priority_R)&(~Blocking_R)&(~Sleeping_R));
#if OS_AGING
  bestIndex = NextTCB_Index;
  for(int i = 0; i < 32; i++){
    ageIndex = CountLeadingZeros((localAge_R)&(~Blocking_R)&(~Sleeping_R));
    if(ageIndex>31){
      break;
    }
    ClearRegisterUNSAFE(&localAge_R, ageIndex);
    tempIndex = ageIndex - (tcb(ageIndex).age>>10);
    NextTCB_Index = tempIndex <= bestIndex ? ageIndex : NextTCB_Index;
    bestIndex = tempIndex <= bestIndex ? tempIndex : bestIndex;
  }
#endif
#if OS_PRI_INVERSION
  bestIndex = NextTCB_Index;
  for(int i = 0; i < 32; i++){
    priInversionIndex = CountLeadingZeros(localPriInversion_R);
    if(priInversionIndex>31){
      break;
    }
    ClearRegisterUNSAFE(&localPriInversion_R, priInversionIndex);
    tempIndex = priInversionIndex - tcb(priInversionIndex).PriInversionInc;
    NextTCB_Index = tempIndex <= bestIndex ? priInversionIndex : NextTCB_Index;
    bestIndex = tempIndex <= bestIndex ? tempIndex : bestIndex;
  }
#endif
  tcb(NextTCB_Index).remainingTimeSlice = tcb(NextTCB_Index).remainingTimeSlice ? 
                                      tcb(NextTCB_Index).remainingTimeSlice : tcb(NextTCB_Index).refillTimeSlice;
  SysTick_SetTime(tcb(NextTCB_Index).remainingTimeSlice);
  TCBFrequency[NextTCB_Index]++;
#if OS_DEBUG
  EndCriticalWrapper(sr);
#else
  EndCritical(sr);
#endif
}

uint32_t OS_Time(void){
  return OS_RunTime;
}

uint32_t OS_TimeDifference(uint32_t start, uint32_t stop){
  return stop - start;
}

void OS_ClearTime(void){
  //OS_RunTime = 0;
}

unsigned long OS_MsTime(void){
  return OS_100UsRunTime/10;
}

void OS_Delay1ms(int i){
  unsigned long start = OS_RunTime;
  i *= 80000;
  while((OS_RunTime-start)<i){};
}

void static OS_RunClock_Init(void){
  SYSCTL_RCGCTIMER_R |= 0x08;   // 0) activate TIMER3
  TIMER3_CTL_R = 0x00000000;    // 1) disable TIMER3A during setup
  TIMER3_CFG_R = 0x00000000;    // 2) configure for 32-bit mode
  TIMER3_TAMR_R = 0x00000012;   // 3) configure for periodic mode, default down-count settings
  TIMER3_TAILR_R = 0xFFFFFFFF-1;    // 4) reload value
  TIMER3_TAPR_R = 0;            // 5) bus clock resolution
  TIMER3_CTL_R = 0x00000001;    // 10) enable TIMER3A
}

void static OS_SleepingAndAgingTimer_Init(void){
  SYSCTL_RCGCTIMER_R |= 0x02;   // 0) activate TIMER1
  TIMER1_CTL_R = 0x00000000;    // 1) disable TIMER1A during setup
  TIMER1_CFG_R = TIMER_CFG_16_BIT;    // 2) configure for 32-bit mode
  TIMER1_TAMR_R = 0x00000002;   // 3) configure for periodic mode, default down-count settings
  TIMER1_TAILR_R = 7999;    // 4) reload value
  TIMER1_TAPR_R = 0;            // 5) bus clock resolution
  TIMER1_ICR_R = 0x00000001;    // 6) clear TIMER1A timeout flag
  TIMER1_IMR_R = 0x00000001;    // 7) arm timeout interrupt
  NVIC_PRI5_R = (NVIC_PRI5_R&0xFFFF00FF)|0x00008000; // 8) priority 4
// interrupts enabled in the main program after all devices initialized
// vector number 37, interrupt number 21
  NVIC_EN0_R = 1<<21;           // 9) enable IRQ 21 in NVIC
  TIMER1_CTL_R = 0x00000001;    // 10) enable TIMER1A
}

void Timer1A_Handler(void){int tempIndex;
  TIMER1_ICR_R = TIMER_ICR_TATOCINT;// acknowledge timer1A timeout
  OS_100UsRunTime+=1;
  uint32_t localSleeping_R = Sleeping_R;
  for(int i = 0; i < 32; i++){
    tempIndex = CountLeadingZeros(localSleeping_R);
    if(tempIndex>=28){
      break;
    }
    ClearRegisterUNSAFE(&localSleeping_R,tempIndex);
    if(tcb(tempIndex).sleep){
      tcb(tempIndex).sleep--;
    }
    else{
      ClearRegister(&Sleeping_R, tempIndex);
    }
  }
#if OS_AGING
  uint32_t localPriority_R = Priority_R;
  for(int i = 0; i < 32; i++){
    tempIndex = CountLeadingZeros(localPriority_R);
    if(tempIndex>31){
      break;
    }
    if((tempIndex==12)||(tempIndex==13)||(tempIndex==31)){
    }
    else{
      tcb(tempIndex).age += 10;
      if(tcb(tempIndex).age>>10){
        SetRegister(&Age_R,tempIndex);        
      }
    }
    ClearRegisterUNSAFE(&localPriority_R,tempIndex);
  }
#endif
  EnableInterrupts();
}

void static OS_Idle(void){
  WaitForInterrupt();
}

uint8_t static OS_AddInterpreter(void){
  int32_t index = 4;
  if(!OS_AllocateTCB(index)){
    return 0;
  }
  tcb(index).name = "Interpreter";
  tcb(index).stackPt = &tcb(index).Regs[0]; //set stack pointer
  tcb(index).PC = INTERPRETER_Run;
  tcb(index).PSR = 0x01000000;
  tcb(index).sleep = 0;
  tcb(index).age = 0;
  tcb(index).refillTimeSlice = OS_DisplayTimeSlice;
  tcb(index).remainingTimeSlice = OS_TimeSlice;
  tcb(index).PriInversionInc = 0;
  tcb(index).Periodic_countdown = 0;
  tcb(index).Periodic_period = 0;
  tcb(index).id = 4;
  SetRegister(&Priority_R,index); ClearRegister(&Blocking_R,index); ClearRegister(&Sleeping_R,index); 
  return 1;
}

uint8_t static OS_AddLCD(void){
  int32_t index = 5;
  if(!OS_AllocateTCB(index)){
    return 0;
  }
  tcb(index).name = "LCD";
  tcb(index).stackPt = &tcb(index).Regs[0]; //set stack pointer
  tcb(index).PC = LCD_Run;
  tcb(index).PSR = 0x01000000;
  tcb(index).sleep = 0;
  tcb(index).age = 0;
  tcb(index).refillTimeSlice = OS_DisplayTimeSlice;
  tcb(index).remainingTimeSlice = OS_TimeSlice;
  tcb(index).PriInversionInc = 0;
  tcb(index).Periodic_countdown = 0;
  tcb(index).Periodic_period = 0;
  tcb(index).id = 5;
  SetRegister(&Priority_R,index); ClearRegister(&Blocking_R,index); ClearRegister(&Sleeping_R,index); 
  return 1;
}

uint8_t static OS_AddIdle(void){
  int32_t index = 31;
  if(!OS_AllocateTCB(index)){
    return 0;
  }
  tcb(index).name = "Idle";
  tcb(index).stackPt = &tcb(index).Regs[0]; //set stack pointer
  tcb(index).PC = OS_Idle;
  tcb(index).PSR = 0x01000000;
  tcb(index).sleep = 0;
  tcb(index).age = 0;
  tcb(index).refillTimeSlice = OS_DisplayTimeSlice;
  tcb(index).remainingTimeSlice = OS_TimeSlice;
  tcb(index).PriInversionInc = 0;
  tcb(index).Periodic_countdown = 0;
  tcb(index).Periodic_period = 0;
  tcb(index).id = 31;
  SetRegister(&Priority_R,index); ClearRegister(&Blocking_R,index); ClearRegister(&Sleeping_R,index); 
  return 1;
}

