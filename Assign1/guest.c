#include <stddef.h>
#include <stdint.h>

/*
	This C function outb takes two arguments, a port number and a value. 
	Note that the x86 EAX and EDX registers are conventionally used to hold arguments for 
	the outb instruction. Therefore, the assembly code in this function loads the value to put 
	out on the port into the EAX register, the port number into the EDX register, and invokes 
	the outb instruction with suitable arguments.
*/


static void outb(uint16_t port, uint8_t value) {
	asm("outb %0,%1" : /* empty */ : "a" (value), "Nd" (port) : "memory");
}


static inline uint32_t inb(uint16_t port) {
  uint8_t ret;
  asm("in %1, %0" : "=a"(ret) : "Nd"(port) : "memory" );
  return ret;
}


/*The function printVal(uint32_t val) should print to the screen the 32-bit value given as argument.*/
/****************** printVal *********************/
void printVal(uint32_t val)
{ 
	outb(0xEA,val);
}

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void) {
	/************ INITIAL HELLO WORLD CODE *******************/
	const char *p;

	for (p = "Hello, world!\n"; *p; ++p)
		outb(0xE9, *p);
	
	
	/************END OF HELLO WORLD CODE *********************/

	/*
		Once the above instruction is executed by the guest, it causes an exit into the
		hypervisor code. The hypervisor checks the reason for the exit and handles it accordingly.
	*/
	
	uint32_t val = inb(0xE9); 
	printVal(val);

	*(long *) 0x400 = 42;
	/*
		In the x86 computer architecture, HLT (halt) is an assembly language instruction 
		which halts the central processing unit (CPU) until the next external interrupt is fired.
		[1] Interrupts are signals sent by hardware devices to the CPU alerting it that an event 
		occurred to which it should react. For example, hardware timers send interrupts to the 
		CPU at regular intervals.
	*/
	for (;;)
		asm("hlt" : /* empty */ : "a" (42) : "memory");
}
