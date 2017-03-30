;/*****************************************************************************/
;/* OSasm.s: low-level OS commands, written in assembly                       */
;/* derived from uCOS-II                                                      */
;/*****************************************************************************/
;Jonathan Valvano, Fixed Scheduler, 5/3/2015


        AREA |.text|, CODE, READONLY, ALIGN=2
        THUMB
        REQUIRE8
        PRESERVE8

        EXPORT  CountLeadingZeros
        EXPORT  OS_AsmLaunch
        EXPORT  PendSV_Handler
        EXPORT  EnterISR
        EXPORT  ExitISRasm
        EXPORT  OS_ISR_Wait
        EXPORT  OS_ISR_Signal
        EXPORT  OS_BlockMe
        EXTERN  TCB_Index
        EXTERN  TCBPointerArray
        EXTERN  NextTCB_Index
        EXTERN  Blocking_R
        EXTERN  OSISR_Stack
        EXTERN  OSISR_NestingCtr  
        EXTERN  OSInterruptTask_R


NVIC_INT_CTRL   EQU     0xE000ED04                              ; Interrupt control state register.
NVIC_SYSPRI14   EQU     0xE000ED22                              ; PendSV priority register (position 14).
NVIC_SYSPRI15   EQU     0xE000ED23                              ; Systick priority register (position 15).
NVIC_LEVEL14    EQU           0xEF                              ; Systick priority value (second lowest).
NVIC_LEVEL15    EQU           0xFF                              ; PendSV priority value (lowest).
NVIC_PENDSVSET  EQU     0x10000000                              ; Value to trigger PendSV exception.
NVIC_ST_RELOAD  EQU     0xE000E014
NVIC_ST_CURRENT EQU     0xE000E018
NVIC_ST_CTRL_R  EQU     0xE000E010

CountLeadingZeros64
    MOV R2, #0
    CMP R1, #0
    MOVEQ R1, R0
    MOVEQ R2, #32
    CLZ R0, R1                 ; Should be passed in R0, and returned in R0
    ADD R0, R0, R2
    BX LR
    

CountLeadingZeros
    CLZ R0, R0                 ; Should be passed in R0, and returned in R0
    BX LR
    



;********************************************************************************************************


OS_AsmLaunch
    LDR R2, =TCB_Index      ; Get address of TCB_Index
    LDR R0, =NextTCB_Index  ; Get address of NextTCB_Index
    LDR R0, [R0]            ; Load next TCB index
    STR R0, [R2]            ; Store NextTCB_Index into TCB_Index
    LDR R0, [R2]            ; Load current TCB index
    LSL R0, R0, #2          ; Multiply Index by 4, for 32-bit addressing
    LDR R1, =TCBPointerArray; Get address of first TCB pointer
    ADD R1, R1, R0          ; Get address of TCB pointer for our index
    LDR R1, [R1]            ; Get address of TCB for our index
    LDR SP, [R1]            ; Load new SP into SP_r
    POP     {R4-R11}           ; restore regs r4-11 
    POP     {R0-R3}            ; restore regs r0-3 
    POP     {R12}
    POP     {LR}               ; discard LR from initial stack
    POP     {R0}               ; start location
    POP     {R1}               ; discard PSR
    CPSIE   I                  ; Enable interrupts at processor level
    BX      R0                 ; start first thread

OSStartHang
    B       OSStartHang        ; Should never get here



;********************************************************************************************************

  
    
PendSV_Handler
    CPSID I                 ; Disable interrupts
    PUSH {R4-R11}           ; Store registers on stack
    LDR R2, =TCB_Index      ; Get address of TCB_Index
    LDR R0, [R2]            ; Load current TCB index
    LSL R0, R0, #2          ; Multiply Index by 4, for 32-bit addressing
    LDR R1, =TCBPointerArray; Get address of first TCB pointer
    ADD R1, R1, R0          ; Get address of TCB pointer for our index
    LDR R1, [R1]            ; Get address of TCB for our index
    STR SP, [R1]            ; Store SP in TCB
    LDR R0, =NextTCB_Index  ; Get address of NextTCB_Index
    LDR R0, [R0]            ; Load next TCB index
    STR R0, [R2]            ; Store NextTCB_Index into TCB_Index
    LDR R0, [R2]            ; Load current TCB index
    LSL R0, R0, #2          ; Multiply Index by 4, for 32-bit addressing
    LDR R1, =TCBPointerArray; Get address of first TCB pointer
    ADD R1, R1, R0          ; Get address of TCB pointer for our index
    LDR R1, [R1]            ; Get address of TCB for our index
    LDR SP, [R1]            ; Load new SP into SP_r
    POP {R4-R11}            ; Load new registers
    LDR R0, =NVIC_ST_CTRL_R
    LDR R1, [R0]
    MOV R2, #1
    ORR R1, R2
    STR R1, [R0]
    CPSIE   I                  ; Enable interrupts at processor level
    BX LR


;********************************************************************************************************

  
EnterISR
    CPSID I                 ; Disable interrupts
    LDR R0, =OSISR_NestingCtr
    LDR R1, [R0]
    SUBS R1, R1, #1
    STR R1, [R0]
    BXMI LR
    LDR R0, =NVIC_ST_CTRL_R ; Disable SysTick
    LDR R1, [R0]
    MOV R2, #0xFFFFFFFE
    AND R1, R2
    STR R1, [R0]
    PUSH {R4-R11}           ; Store registers on stack
    LDR R2, =TCB_Index      ; Get address of TCB_Index
    LDR R0, [R2]            ; Load current TCB index
    LSL R0, R0, #2          ; Multiply Index by 4, for 32-bit addressing
    LDR R1, =TCBPointerArray; Get address of first TCB pointer
    ADD R1, R1, R0          ; Get address of TCB pointer for our index
    LDR R1, [R1]            ; Get address of TCB for our index
    STR SP, [R1]            ; Store SP in TCB
    LDR SP, =OSISR_Stack
    BX LR
    
ExitISRasm
    LDR R0, =OSISR_NestingCtr
    LDR R1, [R0]
    ADDS R1, R1, #1
    STR R1, [R0]
    CMP R1, #1
    MOV R0, #0
    BXNE LR
    LDR R0, =NVIC_ST_CTRL_R    ; Enables SysTick
    LDR R1, [R0]
    MOV R2, #1
    ORR R1, R2
    STR R1, [R0]
    LDR R0, =TCB_Index
    LDR R0, [R0]            ; Load current TCB index
    LSL R0, R0, #2          ; Multiply Index by 4, for 32-bit addressing
    LDR R1, =TCBPointerArray; Get address of first TCB pointer
    ADD R1, R1, R0          ; Get address of TCB pointer for our index
    LDR R1, [R1]            ; Get address of TCB for our index
    LDR SP, [R1]            ; Load new SP into SP_r
    POP {R4-R11}            ; Load new registers
    CPSIE   I                  ; Enable interrupts at processor level
    MOV R0, #1
    BX LR

OS_BlockMe
    LDR R1, =TCB_Index
    LDR R1, [R1]
    MOV R2, #1
    LSL R2, R1
    LDR R3, =Blocking_R
    CPSID I
    LDR R1, [R3]
    ORR R1, R1, R2
    STR R1, [R3]
    CPSIE I
    BX LR
    
OS_ISR_AddToReg
    MOV R2, #1
    LDR R3, =TCB_Index
    LDR R3, [R3]
    LSL R2, R3
    CPSID I
    LDR R1, [R0]
    ORR R1, R1, R2
    STR R1, [R0]
    CPSIE I
    BX LR
    
OS_ISR_UnblockWithReg
    LDR R2, =Blocking_R
    CPSID I
    LDR R3, [R2]
    LDR R1, [R0]
    MVN R1, R1
    AND R3, R3, R1
    STR R3, [R2]
    CPSIE I
    BX LR
    
OS_ISR_Wait
    LDR R1, =TCB_Index
    LDR R1, [R1]
    MOV R2, #1
    LSL R2, R1
    LDR R3, =Blocking_R
    CPSID I
    LDR R1, [R0]
    ORR R1, R1, R2
    STR R1, [R0]
    LDR R1, [R3]
    ORR R1, R1, R2
    STR R1, [R3]
    CPSIE I
    BX LR
    
OS_ISR_Signal
    LDR R1, [R0]
    MOV R2, #0
    STR R2, [R0]
    MVN R0, R1
    LDR R2, =OSInterruptTask_R
    LDR R3, =Blocking_R
    CPSID I
    LDR R12, [R2]
    ORR R12, R12, R1
    STR R12, [R2]
    LDR R12, [R3]
    AND R12, R12, R0
    STR R12, [R3]
    CPSIE I
    BX LR
    

    ALIGN
    END
      
      
