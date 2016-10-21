// os.c
// Runs on LM4F120/TM4C123/MSP432
// Lab 2 starter file.
// Daniel Valvano
// February 20, 2016

#include <stdint.h>
#include "os.h"
#include "../inc/CortexM.h"
#include "../inc/BSP.h"

// function definitions in osasm.s
void StartOS(void);

tcbType tcbs[NUMTHREADS];
tcbType *RunPt;
int32_t Stacks[NUMTHREADS][STACKSIZE];

uint32_t periodic1Rate = 0;
uint32_t periodic2Rate = 0;
void (*periodic1Task)(void) = 0;
void (*periodic2Task)(void) = 0;

uint64_t Ticks = 0;

// ******** OS_Init ************
// Initialize operating system, disable interrupts
// Initialize OS controlled I/O: systick, bus clock as fast as possible
// Initialize OS global variables
// Inputs:  none
// Outputs: none
void OS_Init(void){
  DisableInterrupts();
  BSP_Clock_InitFastest();// set processor clock to fastest speed
  // initialize any global variables as needed
}

void SetInitialStack(int i){
  if (i >= NUMTHREADS)
		return;
	
	int32_t *sp = &Stacks[i][STACKSIZE-16];
	tcbs[i].sp = sp;
	
	*sp     = 0x04040404;  // R4
	*(++sp) = 0x05050505;  // R5
	*(++sp) = 0x06060606;  // R6
	*(++sp) = 0x07070707;  // R7
	*(++sp) = 0x08080808;  // R8
	*(++sp) = 0x09090909;  // R9
	*(++sp) = 0x10101010;  // R10
	*(++sp) = 0x11111111;  // R11
	
	*(++sp) = 0x00000000;  // R0
	*(++sp) = 0x01010101;  // R1
	*(++sp) = 0x02020202;  // R2
	*(++sp) = 0x03030303;  // R3
	*(++sp) = 0x12121212;  // R12
	*(++sp) = 0x14141414;  // LR (R14)
	*(++sp) = 0x00000000;  // Init value of PC doesn't matter
	*(++sp) = 0x01000000;  // Enable thumb bit in PSR
}

//******** OS_AddThreads ***************
// Add four main threads to the scheduler
// Inputs: function pointers to four void/void main threads
// Outputs: 1 if successful, 0 if this thread can not be added
// This function will only be called once, after OS_Init and before OS_Launch
int OS_AddThreads(void(*thread0)(void),
                  void(*thread1)(void),
                  void(*thread2)(void),
                  void(*thread3)(void)){
	for (int i = 0; i < NUMTHREADS; ++i) {
		SetInitialStack(i);
	}

	RunPt = &tcbs[0];
	tcbs[0].next = &tcbs[1];	
	tcbs[1].next = &tcbs[2];
	
	if (thread3) {
		tcbs[2].next = &tcbs[3];
		tcbs[3].next = &tcbs[0];
	} else {
		tcbs[2].next = &tcbs[0];
	}
	
	Stacks[0][STACKSIZE-2] = (int32_t)thread0;
	Stacks[1][STACKSIZE-2] = (int32_t)thread1;
	Stacks[2][STACKSIZE-2] = (int32_t)thread2;
	Stacks[3][STACKSIZE-2] = (int32_t)thread3;
	
  return 1;               // successful
}
									
//******** OS_AddThreads3 ***************
// add three foregound threads to the scheduler
// This is needed during debugging and not part of final solution
// Inputs: three pointers to a void/void foreground tasks
// Outputs: 1 if successful, 0 if this thread can not be added
int OS_AddThreads3(void(*task0)(void),
                 void(*task1)(void),
                 void(*task2)(void)){ 
  return OS_AddThreads(task0, task1, task2, 0);
}
								 
//******** OS_AddPeriodicEventThreads ***************
// Add two background periodic event threads
// Typically this function receives the highest priority
// Inputs: pointers to a void/void event thread function2
//         periods given in units of OS_Launch (Lab 2 this will be msec)
// Outputs: 1 if successful, 0 if this thread cannot be added
// It is assumed that the event threads will run to completion and return
// It is assumed the time to run these event threads is short compared to 1 msec
// These threads cannot spin, block, loop, sleep, or kill
// These threads can call OS_Signal
int OS_AddPeriodicEventThreads(void(*thread1)(void), uint32_t period1,
  void(*thread2)(void), uint32_t period2){
  periodic1Task = thread1;
	periodic1Rate = period1;
  periodic2Task = thread2;
	periodic2Rate = period2;
  return 1;
}

//******** OS_Launch ***************
// Start the scheduler, enable interrupts
// Inputs: number of clock cycles for each time slice
// Outputs: none (does not return)
// Errors: theTimeSlice must be less than 16,777,216
void OS_Launch(uint32_t theTimeSlice){
  STCTRL = 0;                  // disable SysTick during setup
  STCURRENT = 0;               // any write to current clears it
  SYSPRI3 =(SYSPRI3&0x00FFFFFF)|0xE0000000; // priority 7
  STRELOAD = theTimeSlice - 1; // reload value
  STCTRL = 0x00000007;         // enable, core clock and interrupt arm
  StartOS();                   // start on the first task
}
// runs every ms
void Scheduler(void){ // every time slice
  Ticks++;
	
	if (Ticks % periodic1Rate == 0) {
		periodic1Task();
	}
	
	if (Ticks % periodic2Rate == 0) {
		periodic2Task();
	}
	
	RunPt = RunPt->next;
}

// ******** OS_InitSemaphore ************
// Initialize counting semaphore
// Inputs:  pointer to a semaphore
//          initial value of semaphore
// Outputs: none
void OS_InitSemaphore(int32_t *semaPt, int32_t value){
	long crit = StartCritical();
  *semaPt = value;
	EndCritical(crit);
}

// ******** OS_Wait ************
// Decrement semaphore
// Lab2 spinlock (does not suspend while spinning)
// Lab3 block if less than zero
// Inputs:  pointer to a counting semaphore
// Outputs: none
void OS_Wait(int32_t *semaPt){
	uint32_t val = 0;
	long crit = 0;
	do {
		crit = StartCritical();
		val = *semaPt;
		EndCritical(crit);
	} while (val == 0);
	crit = StartCritical();
	*semaPt = *semaPt - 1;
	EndCritical(crit);
}

// ******** OS_Signal ************
// Increment semaphore
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate
// Inputs:  pointer to a counting semaphore
// Outputs: none
void OS_Signal(int32_t *semaPt){
	long crit = StartCritical();
  *semaPt = *semaPt + 1;
	EndCritical(crit);
}




// ******** OS_MailBox_Init ************
// Initialize communication channel
// Producer is an event thread, consumer is a main thread
// Inputs:  none
// Outputs: none
int32_t Mail;
uint32_t MailData = 0;
void OS_MailBox_Init(void){
  // include data field and semaphore
  OS_InitSemaphore(&Mail, 0);
}

// ******** OS_MailBox_Send ************
// Enter data into the MailBox, do not spin/block if full
// Use semaphore to synchronize with OS_MailBox_Recv
// Inputs:  data to be sent
// Outputs: none
// Errors: data lost if MailBox already has data
void OS_MailBox_Send(uint32_t data){
  long crit = StartCritical();
	MailData = data;
	EndCritical(crit);
	OS_Signal(&Mail);
}

// ******** OS_MailBox_Recv ************
// retreive mail from the MailBox
// Use semaphore to synchronize with OS_MailBox_Send
// Lab 2 spin on semaphore if mailbox empty
// Lab 3 block on semaphore if mailbox empty
// Inputs:  none
// Outputs: data retreived
// Errors:  none
uint32_t OS_MailBox_Recv(void){
	uint32_t data;
	OS_Wait(&Mail);
  long crit = StartCritical();
	data = MailData;
	EndCritical(crit);
  return data;
}



