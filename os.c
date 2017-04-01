#include "os.h"
#include "FIFO.h"
#include "SysTick.h"
#include "../inc/tm4c123gh6pm.h"
#include "SysTick.h"
#include "interpreter.h"
#include "pll.h"
#include "uart.h"
#include "st7735.h"
#include "ff.h"
#include "heap.h"

#define TRUE  1
#define FALSE 0

#define ROUND_ROBIN_SCH   TRUE
#define PRIORITY_SCH      FALSE
#define POSTOFFICESIZE 32
#define FIFOSIZE   128         // size of the FIFOs (must be power of 2)
#define FIFOSUCCESS 1         // return value on success
#define FIFOFAIL    0         // return value on failure
                              // create index implementation FIFO (see FIFO.h)
#define MAILBOXSIZE 30

struct TCB * PLR = 0;

static uint8_t OS_FIFO_Index;

static Sema4Type FIFO_Free;
static Sema4Type FIFO_Valid;

uint8_t (*SV_0)(void) = &OS_Id; 
void (*SV_1)(void) = &OS_Kill;
void (*SV_2)(unsigned long) = &OS_Sleep;
unsigned long (*SV_3)(void) = &OS_MsTime;
int (*SV_4)(void(void), uint8_t, uint8_t ) = &OS_AddThread;
uint8_t (**SV_Funcs)(void) = &SV_0;

AddIndexFifo(OS, FIFOSIZE, unsigned long, FIFOSUCCESS, FIFOFAIL)

AddIndexFifo(POSTOFFICE, POSTOFFICESIZE, Mail, FIFOSUCCESS, FIFOFAIL);

static uint8_t OS_MAIL_Index;

static Sema4Type MailFree;
static Sema4Type MailValid;

void static OS_EndProcess(void);

void OS_Fifo_Init(){
  OSFifo_Init();
  FIFO_Free.Value = 128;
  FIFO_Valid.Value = 0;
}

int OS_Fifo_Put(unsigned long data){
  if(FIFO_Free.Value > 0){
    OS_Wait(&FIFO_Free);
    DisableInterrupts();
    OSFifo_Put(data);
    EnableInterrupts();
    OS_Signal(&FIFO_Valid);
    
    return FIFOSUCCESS;
  }
  return FIFOFAIL;
}

unsigned long OS_Fifo_Get(void){
  
  unsigned long ret;
  OS_Wait(&FIFO_Valid);
  DisableInterrupts();
  OSFifo_Get(&ret);
  OS_FIFO_Index--;
  EnableInterrupts();
  OS_Signal(&FIFO_Free);
  return ret;
}

unsigned long OS_Fifo_Size(void){
  return FIFO_Free.Value;
}

void OS_MailBox_Init(){
  MailFree.Value = 32;
  MailValid.Value = 0;
  POSTOFFICEFifo_Init();
}

void OS_MailBox_Send(int device, int line, char* message){
  int i;
  Mail NextMail;
  NextMail.device = device;
  NextMail.line = line;
  for(i = 0; i < 20; i++){
      NextMail.message[i] = message[i];
  }
  
  OS_Wait(&MailFree);
  DisableInterrupts();
  POSTOFFICEFifo_Put(NextMail);
  EnableInterrupts();
  OS_Signal(&MailValid);
}

Mail OS_MailBox_Recv(int i){
  Mail ret;
  OS_Wait(&MailValid);
  DisableInterrupts();
  POSTOFFICEFifo_Get(&ret);
  EnableInterrupts();
  OS_Signal(&MailFree);
  return ret;
}

int OS_MailBox_Count(){
  return MailValid.Value;
}
TCBType* RunPt;
TCBType* NextPt;

const uint8_t TCB_COUNT = 10;
TCBType tcbs[TCB_COUNT];
static uint32_t periodicTimes[5];
static uint32_t periods[5];
static void (*PERIODICTASKS[5])(void);
static uint8_t periodicIndex;


void OS_Wait(Sema4Type *semaPt){
  DisableInterrupts();
  while(semaPt->Value <=0){
    EnableInterrupts();
    DisableInterrupts();
    OS_Suspend();
  }
  semaPt->Value = semaPt->Value - 1;
  EnableInterrupts();
  
}

void OS_Signal(Sema4Type *semaPt){
  long status;
  status = StartCritical();
  semaPt->Value = semaPt->Value + 1;
  EndCritical(status);
  
}

void OS_bWait(Sema4Type *semaPt){
  DisableInterrupts();
  while(semaPt->Value != 1){
    EnableInterrupts();
    DisableInterrupts();
    OS_Suspend();
  }
  semaPt->Value = 0;
  EnableInterrupts();
  
}

void OS_bSignal(Sema4Type *semaPt){
  long status;
  status = StartCritical();
  semaPt->Value = 1;
  EndCritical(status);
}

void OS_InitSemaphore(Sema4Type *semaPt, long value){
  semaPt->Value = value;
}

void OS_Scheduler(void){
#if PRIORITY_SCH
  //write priority scheduler here
#endif
#if ROUND_ROBIN_SCH
  NextPt = 0;
  struct TCB* next = RunPt->next;
  while(NextPt == 0){
    if(next->sleep == 0){
      NextPt = next;
      return;
    }
    else{
      if(SYSTICK_getCount(next->id) >= next->sleep)
      {
        next->sleep = 0;
        NextPt = next;
      }
      else{
        next = next->next;
      }
    }
  }
#endif //ROUND_ROBIN_SCH
}


void OS_Suspend(void){
  //call scheduler
  OS_Scheduler();
  NVIC_INT_CTRL_R = 0x10000000; // Trigger PendSV
}

uint8_t OS_Id(void){
  return RunPt->id;
}

void OS_Kill(void){
  struct TCB* new_prev = RunPt->prev;
  struct TCB* new_next = RunPt->next;
  
  //properly link the tasks before and after the current
  new_next->prev = new_prev;
  new_prev->next = new_next;
  
  //set current running id to 0 for killed
  RunPt->id = 0;
  
  //call os suspend to context switch to the next tasks
  //OS_Suspend();
}
int OS_AddSW1Task(void(*pushTask)(void), void(*pullTask)(void), unsigned long priority){
  Switch_Init(pushTask, pullTask);
}
int OS_AddThread(void(*task)(void),  uint8_t priority, uint8_t id){
  uint8_t i;
	for(i = TCB_COUNT; i > 0; i--){
		if(tcbs[i-1].id == 0){
			tcbs[i-1].id = id; //set id
			tcbs[i-1].stackPt = &tcbs[i-1].Regs[0]; //set stack pointer
			tcbs[i-1].PC = task;
			tcbs[i-1].PSR = 0x01000000;
			if(RunPt == 0){
				tcbs[i-1].next = &tcbs[i-1];
        tcbs[i-1].prev = &tcbs[i-1];
				RunPt = &tcbs[i-1];
			}
			else{
				struct TCB* placeHolder = RunPt->next;
				RunPt->next = (&tcbs[i-1]);
				tcbs[i-1].next = placeHolder;
        tcbs[i-1].prev = RunPt;
        placeHolder->prev = &tcbs[i-1];
			}
			return 1;
		}
	}
	//no empty tcb found
	return -1;
}

void OS_Sleep(unsigned long sleepTime){
  RunPt->sleep = sleepTime;
  SYSTICK_setCount(RunPt->id);
  OS_Suspend();
}
// ***************** TIMER1_Init ****************
// Activate TIMER1 for OS_Time clock cycles
// Inputs:  none
// Outputs: none
void Timer1_Init(void){
  SYSCTL_RCGCTIMER_R |= 0x02;   // 0) activate TIMER1
  TIMER1_CTL_R = 0x00000000;    // 1) disable TIMER1A during setup
  TIMER1_CFG_R = 0x00000000;    // 2) configure for 32-bit mode
  TIMER1_TAMR_R = TIMER_TAMR_TACDIR; // 3) configure for count up
  TIMER1_TAPR_R = 0;            // 5) bus clock resolution
  TIMER1_TAR_R = 0;
  TIMER1_CTL_R = 0x00000001;    // 10) enable TIMER1A
}

// ***************** OS_Time ****************
// Runtime in clock cycles
// Inputs:  none
// Outputs: runtime
unsigned long OS_Time(void){
  return TIMER1_TAR_R;
}

unsigned long OS_TimeDifference(unsigned long start, unsigned long stop){
  return stop-start;
}

int Timer2Period;

void Timer2_Init(unsigned long period){
  Timer2Period = period;
  SYSCTL_RCGCTIMER_R |= 0x04;   // 0) activate timer2
  TIMER2_CTL_R = 0x00000000;    // 1) disable timer2A during setup
  TIMER2_CFG_R = 0x00000000;    // 2) configure for 32-bit mode
  TIMER2_TAMR_R = 0x00000002;   // 3) configure for periodic mode, default down-count settings
  TIMER2_TAILR_R = period-1;    // 4) reload value
  TIMER2_TAPR_R = 0;            // 5) bus clock resolution
  TIMER2_ICR_R = 0x00000001;    // 6) clear timer2A timeout flag
  TIMER2_IMR_R = 0x00000001;    // 7) arm timeout interrupt
  NVIC_PRI5_R = (NVIC_PRI5_R&0x00FFFFFF)|0x80000000; // 8) priority 4
// interrupts enabled in the main program after all devices initialized
// vector number 39, interrupt number 23
  NVIC_EN0_R = 1<<23;           // 9) enable IRQ 23 in NVIC
  TIMER2_CTL_R = 0x00000001;    // 10) enable timer2A
}

void Timer2A_Handler(void){
  int i; 
  TIMER2_ICR_R = TIMER_ICR_TATOCINT;// acknowledge TIMER2A timeout
  for(i = 0; i < periodicIndex; i++){
    periodicTimes[i] += Timer2Period;
   
    if(periodicTimes[i] >= periods[i]){
      PERIODICTASKS[i]();
      periodicTimes[i] = 0;
    }
  }
}

int OS_AddPeriodicThread(void(*task)(void), unsigned long period, unsigned long priority){
  if(periodicIndex < 5){
    PERIODICTASKS[periodicIndex] = task;
    periods[periodicIndex] = period;
    periodicTimes[periodicIndex] = 0;
    periodicIndex++;  
    return 1;
  }
  return 0;
}
void PortB_Init(void){ unsigned long volatile delay;
  SYSCTL_RCGC2_R |= 0x02;       // activate port B
  delay = SYSCTL_RCGC2_R;        
  delay = SYSCTL_RCGC2_R;         
  GPIO_PORTB_DIR_R |= 0x3C;    // make PB5-2 output heartbeats
  GPIO_PORTB_AFSEL_R &= ~0x3C;   // disable alt funct on PB5-2
  GPIO_PORTB_DEN_R |= 0x3C;     // enable digital I/O on PB5-2
  GPIO_PORTB_PCTL_R = ~0x00FFFF00;
  GPIO_PORTB_AMSEL_R &= ~0x3C;      // disable analog functionality on PB
}
static FATFS g_sFatFs;
void OS_Init(void){
  uint8_t i;
  RunPt = 0;
  periodicIndex = 0;
  PLL_Init(Bus80MHz);       // set system clock to 50 MHz
  UART_Init();              // initialize UART
  INTERPRETER_initArray();
  PortB_Init();
  Heap_Init();
  ST7735_ds_InitR(INITR_REDTAB, 4, 4, 4, 4);
  f_mount(&g_sFatFs, "", 1);
  Timer2_Init(10000); 
  DisableInterrupts();//run the periodic functions ever 0.000125 seconds - 8 KHz
	for(i = TCB_COUNT; i > 0; i--){
    tcbs[i-1].PSR = 0x01000000;
    tcbs[i-1].id = 0;
    tcbs[i-1].PC = (void *) 0x15151515;
    tcbs[i-1].Regs[0] = 0x44444444;
    tcbs[i-1].Regs[1] = 0x55555555;
    tcbs[i-1].Regs[2] = 0x66666666;
    tcbs[i-1].Regs[3] = 0x77777777;
    tcbs[i-1].Regs[4] = 0x88888888;
    tcbs[i-1].Regs[5] = 0x99999999;
    tcbs[i-1].Regs[6] = 0x10101010;
    tcbs[i-1].Regs[7] = 0x11111110;
    tcbs[i-1].Regs[8] = 0x00000000;
    tcbs[i-1].Regs[9] = 0x11111111;
    tcbs[i-1].Regs[10] = 0x22222222;
    tcbs[i-1].Regs[11] = 0x33333333;
    tcbs[i-1].Regs[12] = 0x12121212;
    tcbs[i-1].Regs[13] = 0x13370000;
    tcbs[i-1].stackPt = &tcbs[i-1].Regs[0];
  }
  //TIMER5_CTL_R = 0x00000000; 
  NVIC_SYS_PRI3_R |= 7<<21;
  
  
  Timer1_Init(); //This is OS time
}

void OS_ClearMsTime(void){
  Timer1_Init();
}
unsigned long OS_MsTime(void){
  return TIMER1_TAR_R/80000;
  
}

void OS_Idle(void){
  while(1){
    if((RunPt->next == RunPt) && (PLR != 0)){
      OS_EndProcess();
    }
    OS_Suspend();
  }
}

void OS_LaunchSystem(unsigned long theTimeSlice){
  SysTick_Init(theTimeSlice);
  OS_Launch();
}

//Should only be called by OS_Idle, when OS_Idle is the only remaining thread
void static OS_EndProcess(void){
  if((RunPt->next == RunPt) && (PLR != 0)){ //check conditions
    RunPt->id = 0; // lazy kill idle
    NextPt = PLR;  // where to return to
    PLR = 0;       // no longer nested
    NVIC_INT_CTRL_R = 0x10000000; // Trigger PendSV to switch threads... and also processes
  }
}

//entry is starting PC, text and data are pointers to malloced data (must be freed upon ending)
//sr is used to reenable the SYSTICK (see below, like enter and exit critial)
int OS_AddProcess(entry_t entry, void* text, void* data, uint8_t priority, uint8_t id, long sr){
  OS_UnLockScheduler(sr);
  // MATTHEW'S NOTES:
  //get a PCB and set it up with the id, priority, and pointers
  //set a TCB up with the "entry" pointer as the PC
  //set a TCB up with the idle task and link the two together
  //set next pointer as the idle and os_suspend (this will switch to the main thread from the idle one
  //set thread count to 1
  //note: TCB's now need a pointer to parent PCB
  //note: Add thread needs to increment parent PCB's threadcount
  //note: kill decrements
  //note: on PCB thread_count == 1 (just idle) kill the process, unmalloc memory, return to system process
  //set a resume point before os_suspend: ResumeSysProc = RunPt;
  
  // ANDREW'S NOTES:
  // I don't use text, data, priority, or id. I'm leaving them, because we might need them later
  
  if(!PLR){ //Check that this isn't a double nested Process i.e. there are only 2 allowed
    return 0;
  }
  PLR = RunPt; // save where to return
  RunPt = 0;   // necessary for AddThread to create an unconnected linked list
  OS_AddThread(entry, 0xFF, 2); // We will start with this thread
  OS_AddThread(OS_Idle, 0xFF, 1); // In case all sleep, block, or are killed
  NextPt = RunPt; // Necessary for PendSV
  RunPt = PLR; // Necessary for PendSV
  NVIC_INT_CTRL_R = 0x10000000; // Trigger PendSV to switch threads... and also processes
  
  EnableInterrupts(); // The entire process should happen here <-
  
  // This only happens AFTER the process has happened, 
  //  has been killed, and we're returning to the original process
  Heap_Free(text); // Deallocate memory
  Heap_Free(data);
  return 1; // Indicate success
}

unsigned long OS_LockScheduler(void){
  unsigned long old = NVIC_ST_CTRL_R;
  NVIC_ST_CTRL_R = NVIC_ST_CTRL_ENABLE+NVIC_ST_CTRL_CLK_SRC;
  return old;
}
void OS_UnLockScheduler(unsigned long previous){
  NVIC_ST_CTRL_R = previous;
}


//PCB ideas
/*
struct PCB{
  void *text;
  void *data
  uint8_t priority
  uint8_t id
  uint8_t thread_count
}

TCB * ResumeSysProc could be the pointer to resume the running task

have a global process count - if trying to add when it is equal to 2 already, don't!
process kill is triggered at the end of OS_Kill if the process thread_count ==1
process kill: free(text), free(data), set other variables to 0, set RunPt to ResumeSysProc
could write a function to be called here that just jumps to the PC value of the TCB pointed to in ResumeSysProc
These are all ideas and you could have a better way!

Note: we have 10 TCB's statically allocated: main needs 3 and added process needs 3, so no need to dynamically allocate/declare more
Note: the idle thread is added because the Proc5.axf functions will both sleep at the same time, system can't crash
I will finish the SVC calls and test them before we meet tomorrow
*/