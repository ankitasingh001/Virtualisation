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

/*The function getNumExists() should return the number of exits incurred by the guest since it started.*/
/****************** getNumExists *********************/
uint32_t getNumExits()
{
	uint32_t val = inb(0xEB);
	return val;
}

/*The function display(const char *str) should print the argument string to the screen via a hypercall*/
/***************** display(const char *str) **************/
void display(const char *str)
{
	outb(0xEC,((uintptr_t)str));
}

/***************** FILE SYSTEM CALLS *************************/

/* This function will open a file and return the file descriptor */
static uint32_t open_file(const char *str) 
{
    outb(0xED,((uintptr_t)str));
    uint8_t ret_val = inb(0xED);
	return ret_val;
}
 
/* This function will read a file given file descriptor and copy contents to a buffer */
static void read_file(uint32_t file_Desc, uint32_t numbytes, uint32_t *buffer) 
{
	/* Creating an array of pointers to store the info */
	/* Pass address of this array to the hypervisor*/

    uint32_t *file_info_read;
    file_info_read[0] = file_Desc;
    file_info_read[1] = (uintptr_t) buffer;
    file_info_read[2] = numbytes;

    outb(0xEE,((uintptr_t)file_info_read));
}

/* This function will write to a file given file descriptor and copy contents to a buffer */
static void write_file(uint32_t file_Desc, uint32_t numbytes, uint32_t *buffer) 
{

    uint32_t *file_info_write;
    file_info_write[0] = file_Desc;
    file_info_write[1] = (uintptr_t) buffer;
    file_info_write[2] = numbytes;

    outb(0xEF,((uintptr_t)file_info_write));
}

/* This function will write to a file given file descriptor and copy contents to a buffer */
static void seek_file(uint32_t file_Desc, uint32_t index) 
{
    uint32_t *file_info_seek;
    file_info_seek[0] = file_Desc;
    file_info_seek[1] = index;

    outb(0xF0,((uintptr_t)file_info_seek));
}


void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void) {
	/************ INITIAL HELLO WORLD CODE *******************/
	const char *p;

	// for (p = "Hello, world!\n"; *p; ++p)
	// 	outb(0xE9, *p);
	
	
	/************END OF HELLO WORLD CODE *********************/

	/*
		Once the above instruction is executed by the guest, it causes an exit into the
		hypervisor code. The hypervisor checks the reason for the exit and handles it accordingly.
	*/

	/* ***************** PRINT & DISPLAY VALUES ****************/

	/* For printing 32 bit value passed, port used = 0xE9*/
	uint32_t val = inb(0xE9); 
	printVal(val);

	/* Calculating number of exits */
	uint32_t numExits = getNumExits();
	printVal(numExits);

	// /* Passing entire string in one go */
	char * strng1 = "Hello There!";

	/* Print string 1 and the number of calls before and after*/
	display(strng1); //1 call for this
	numExits = getNumExits(); //1 call for this
	printVal(numExits);	//1 call for this
	

	/* ********************** FILE OPERATIONS ****************** */

	/********** Open file and return file descriptor *************/
	char * const testFile = "t.txt";
	strng1 ="t.txt";
    uint32_t file_descp = open_file(testFile);
    //printVal(file_descp);

	/******** Read file and display the contents ******************/

	char *read_buffer;
    read_file(file_descp, 10, (uint32_t *)read_buffer);
    display(read_buffer);

	/**************** Write to a file *******************************/

	/* Writing to the same file */

	//char *write_buffer = "She codes";
	const char *ptr;
	strng1 = "This is ankita. Writing this to the file";
	int len =0;
	for (ptr = strng1; *ptr; ++ptr,++len);
	write_file(file_descp, (uint32_t)len, (uint32_t*)&strng1[0]);

	/*********** Performing seeks ****************************/

	/* Seek the contents of the above file from location =loc*/

	int loc =3;
	seek_file(file_descp, loc);

	//char *read_buff;
	read_file(file_descp, 4, (uint32_t *)strng1);
	display(strng1);

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
