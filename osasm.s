;/*****************************************************************************/
; OSasm.s: low-level OS commands, written in assembly                       */
; Runs on LM4F120/TM4C123/MSP432
; Lab 2 starter file
; February 10, 2016
;


        AREA |.text|, CODE, READONLY, ALIGN=2
        THUMB
        REQUIRE8
        PRESERVE8

        EXTERN  RunPt            ; currently running thread
        EXPORT  StartOS
        EXPORT  SysTick_Handler
        IMPORT  Scheduler



SysTick_Handler          ; Saves R0-R3,R12,LR,PC,PSR
    CPSID   I            ; Prevent interrupt during switch
	PUSH    {R4-R11}     ; Push rest of registers onto stack
	LDR     R0, =RunPt   ; Load addr of RunPt into R0
	LDR     R1, [R0]     ; Load value of RunPt into R1
	STR     SP, [R1]     ; Save stack pointer to TCB
	PUSH    {R0,LR}      ; Save the magic number stored in LR
	BL      Scheduler    ; Normal branch to call void Scheduler(void)
	POP     {R0,LR}      ; POP LR off stack
	LDR     R1, [R0]     ; Get the updated RunPt value into R1
	LDR     SP, [R1]     ; Load new TCB's stack pointer into CPU
	POP     {R4-R11}     ; Pop R4 - R11 from stack
    CPSIE   I            ; Tasks run with interrupts enabled
    BX      LR           ; Restore R0-R3,R12,LR,PC,PSR

StartOS
	LDR     R0, =RunPt		   ; Load addr of RunPt into R0
	LDR     R1, [R0]		   ; Load value of RunPt into R1
	LDR     SP, [R1]		   ; Load thread's SP from TCB into CPU SP
	POP     {R4-R11}		   ; Pop R4 - R11 from new SP (SP now points to R0 location)
	POP     {R0-R3}			   ; Pop R0 - R3 from SP (SP now points to R12 location)
	POP     {R12}			   ; Pop R12 (SP now points to LR (R14))
	ADD     SP, SP, #4		   ; Skip LR by incrementing SP (SP now points to PC)
						       ;   - LR is invalid since this is the initial run
	POP     {LR}			   ; Pop PC (R15) into LR (SP now points to PSR)
						       ;  - PC should have already been initialized to start of task’s function
						       ;  - Popping directly into LR means that task will run when we BX LR
	ADD     SP, SP, #4	       ; Skip PSR by incr. SP (SP now points to bottom of task’s stack)
    CPSIE   I                  ; Enable interrupts so SysTick can preempt threads
    BX      LR                 ; Jump to beginning of task 0

    ALIGN
    END
