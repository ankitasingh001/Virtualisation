#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <linux/kvm.h>

/* CR0 bits */
#define CR0_PE 1u
#define CR0_MP (1U << 1)
#define CR0_EM (1U << 2)
#define CR0_TS (1U << 3)
#define CR0_ET (1U << 4)
#define CR0_NE (1U << 5)
#define CR0_WP (1U << 16)
#define CR0_AM (1U << 18)
#define CR0_NW (1U << 29)
#define CR0_CD (1U << 30)
#define CR0_PG (1U << 31)

/* CR4 bits */
#define CR4_VME 1
#define CR4_PVI (1U << 1)
#define CR4_TSD (1U << 2)
#define CR4_DE (1U << 3)
#define CR4_PSE (1U << 4)
#define CR4_PAE (1U << 5)
#define CR4_MCE (1U << 6)
#define CR4_PGE (1U << 7)
#define CR4_PCE (1U << 8)
#define CR4_OSFXSR (1U << 8)
#define CR4_OSXMMEXCPT (1U << 10)
#define CR4_UMIP (1U << 11)
#define CR4_VMXE (1U << 13)
#define CR4_SMXE (1U << 14)
#define CR4_FSGSBASE (1U << 16)
#define CR4_PCIDE (1U << 17)
#define CR4_OSXSAVE (1U << 18)
#define CR4_SMEP (1U << 20)
#define CR4_SMAP (1U << 21)

#define EFER_SCE 1
#define EFER_LME (1U << 8)
#define EFER_LMA (1U << 10)
#define EFER_NXE (1U << 11)

/* 32-bit page directory entry bits */
#define PDE32_PRESENT 1
#define PDE32_RW (1U << 1)
#define PDE32_USER (1U << 2)
#define PDE32_PS (1U << 7)

/* 64-bit page * entry bits */
#define PDE64_PRESENT 1
#define PDE64_RW (1U << 1)
#define PDE64_USER (1U << 2)
#define PDE64_ACCESSED (1U << 5)
#define PDE64_DIRTY (1U << 6)
#define PDE64_PS (1U << 7)
#define PDE64_G (1U << 8)

/*Added by Ankita for guest memory mapping extraction*/
#define EXTRACT_GUEST 0x000000ff

/*
* INFO ABOUT ioctl COMMAND
* Second answer of the following link:
* https://stackoverflow.com/questions/15807846/ioctl-linux-device-driver#:~:text=An%20ioctl%20%2C%20which%20means%20%22input,application%20to%20send%20it%20orders.
*/
struct vm {
	int sys_fd; //fd = file descriptor
	int fd;
	char *mem;
};






void vm_init(struct vm *vm, size_t mem_size)
{
	int api_ver;

	/*  This is the declaration for KVM_SET_USER_MEMORY_REGION present in kvm.h 
	EXPLAINATION::::::::
	And finally tell the KVM virtual machine about its spacious new 4096-byte memory:

		struct kvm_userspace_memory_region region = 
		{
		.slot = 0,
		.guest_phys_addr = 0x1000,
		.memory_size = 0x1000,
		.userspace_addr = (uint64_t)mem,
		};

		ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &region);

	The slot field provides an integer index identifying each region of memory we
	hand to KVM; calling KVM_SET_USER_MEMORY_REGION again with the same slot will replace 
	this mapping, while calling it with a new slot will create a separate mapping. 
	guest_phys_addr specifies the base "physical" address as seen from the guest, and
	userspace_addr points to the backing memory in our process that we allocated with
	mmap(); note that these always use 64-bit values, even on 32-bit platforms. 
	memory_size specifies how much memory to map: one page, 0x1000 bytes. 
 */

	struct kvm_userspace_memory_region memreg;

	/* We need read-write access to the device to set up a virtual machine, 
		and all opens not explicitly intended for inheritance across exec should 
		use O_CLOEXEC.

		Depending on your system, you likely have access to /dev/kvm either
		via a group named "kvm" or via an access control list (ACL) 
		granting access to users logged in at the console.
	 */
	
	vm->sys_fd = open("/dev/kvm", O_RDWR);
	if (vm->sys_fd < 0) {
		perror("open /dev/kvm");
		exit(1);
	}

	/* Check if we have appropriate version number (12) or not*/

	api_ver = ioctl(vm->sys_fd, KVM_GET_API_VERSION, 0);
	if (api_ver < 0) {
		perror("KVM_GET_API_VERSION");
		exit(1);
	}

	if (api_ver != KVM_API_VERSION) {
		fprintf(stderr, "Got KVM api version %d, expected %d\n",
			api_ver, KVM_API_VERSION);
		exit(1);
	}
	/* 
		Next, we need to create a virtual machine (VM), which represents everything 
		associated with one emulated system, including memory and one or more CPUs.
		KVM gives us a handle to this VM in the form of a file descriptor: 
	*/
	vm->fd = ioctl(vm->sys_fd, KVM_CREATE_VM, 0);

	if (vm->fd < 0) {
		perror("KVM_CREATE_VM");
		exit(1);
	}

	if (ioctl(vm->fd, KVM_SET_TSS_ADDR, 0xfffbd000) < 0) {
		perror("KVM_SET_TSS_ADDR");
	exit(1);
	}
/* 	   
	void *mmap(void *addr, size_t length, int prot, int flags,int fd, off_t offset);
    int munmap(void *addr, size_t length);
		In computing, mmap(2) is a POSIX-compliant Unix system call that maps files 
		or devices into memory. It is a method of memory-mapped file I/O.
		It implements demand paging because file contents are not read from disk 
		directly and initially do not use physical RAM at all. The actual reads
		from disk are performed in a "lazy" manner, after a specific location 
		is accessed. After the memory is no longer needed, it is important to 
		munmap(2) the pointers to it. Protection information can be managed using mprotect(2), 
		and special treatment can be enforced using madvise(2).


	   mmap() creates a new mapping in the virtual address space of the
       calling process.  The starting address for the new mapping is
       specified in addr.  The length argument specifies the length of
       the mapping (which must be greater than 0).

       If addr is NULL, then the kernel chooses the (page-aligned)
       address at which to create the mapping; this is the most portable
       method of creating a new mapping.  If addr is not NULL, then the
       kernel takes it as a hint about where to place the mapping; on
       Linux, the kernel will pick a nearby page boundary (but always
       above or equal to the value specified by
       /proc/sys/vm/mmap_min_addr) and attempt to create the mapping
       there.  If another mapping already exists there, the kernel picks
       a new address that may or may not depend on the hint.  The
       address of the new mapping is returned as the result of the call.

       The contents of a file mapping (as opposed to an anonymous
       mapping; see MAP_ANONYMOUS below), are initialized using length
       bytes starting at offset offset in the file (or other object)
       referred to by the file descriptor fd.  offset must be a multiple
       of the page size as returned by sysconf(_SC_PAGE_SIZE).

       After the mmap() call has returned, the file descriptor, fd, can
       be closed immediately without invalidating the mapping.

       The prot argument describes the desired memory protection of the
       mapping (and must not conflict with the open mode of the file).
       It is either PROT_NONE or the bitwise OR of one or more of the
       following flags:

       PROT_EXEC
              Pages may be executed.

       PROT_READ
              Pages may be read.

       PROT_WRITE
              Pages may be written.

       PROT_NONE
              Pages may not be accessed. */

	/*How and where in the hyprevisor code is this guest memory allocated from the host OS? */
	vm->mem = mmap(NULL, mem_size, PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
	if (vm->mem == MAP_FAILED) {
		perror("mmap mem");
		exit(1);
	}
	

/* 	int madvise(void *addr, size_t length, int advice);

	The madvise() system call is used to give advice or directions to
       the kernel about the address range beginning at address addr and
       with size length bytes In most cases, the goal of such advice is
       to improve system or application performance.

       Initially, the system call supported a set of "conventional"
       advice values, which are also available on several other
       implementations.  (Note, though, that madvise() is not specified
       in POSIX.)  Subsequently, a number of Linux-specific advice
       values have been added.

	   MADV_MERGEABLE (since Linux 2.6.32)
              Enable Kernel Samepage Merging (KSM) for the pages in the
              range specified by addr and length.  The kernel regularly
              scans those areas of user memory that have been marked as
              mergeable, looking for pages with identical content.
              These are replaced by a single write-protected page (which
              is automatically copied if a process later wants to update
              the content of the page).  KSM merges only private
              anonymous pages (see mmap(2)).

              The KSM feature is intended for applications that generate
              many instances of the same data (e.g., virtualization
              systems such as KVM).  It can consume a lot of processing
              power; use with care.  See the Linux kernel source file
              Documentation/admin-guide/mm/ksm.rst for more details.

              The MADV_MERGEABLE and MADV_UNMERGEABLE operations are
              available only if the kernel was configured with
              CONFIG_KSM.
 */


	madvise(vm->mem, mem_size, MADV_MERGEABLE);

	memreg.slot = 0;
	memreg.flags = 0;
	memreg.guest_phys_addr = 0; //Physical address of guest OS
	memreg.memory_size = mem_size;
	memreg.userspace_addr = (unsigned long)vm->mem;  //Address of process = virtual address of host OS
        if (ioctl(vm->fd, KVM_SET_USER_MEMORY_REGION, &memreg) < 0) {
		perror("KVM_SET_USER_MEMORY_REGION");
                exit(1);
	}
}

struct vcpu {
	int fd;
	struct kvm_run *kvm_run;
};

void vcpu_init(struct vm *vm, struct vcpu *vcpu)
{
	int vcpu_mmap_size;
	/*
		Virtual CPU to run code.A KVM virtual CPU represents the state of one emulated CPU,
		including processor registers and other execution state.
		Again, KVM gives us a handle to this VCPU in the form of a file descriptor

	  */
	vcpu->fd = ioctl(vm->fd, KVM_CREATE_VCPU, 0);
	/* 
		The 0 here represents a sequential virtual CPU index. 
		A VM with multiple CPUs would assign a series of small identifiers here, 
		from 0 to a system-specific limit (obtainable by checking the KVM_CAP_MAX_VCPUS 
		capability with KVM_CHECK_EXTENSION).
 	*/


        if (vcpu->fd < 0) {
		perror("KVM_CREATE_VCPU");
                exit(1);
	}

	/*
		KVM_GET_VCPU_MMAP_SIZE
 	* Get size for mmap(vcpu_fd)
 	*/

 	/*
		Each virtual CPU has an associated struct kvm_run data structure, used to communicate 
		information about the CPU between the kernel and user space. In particular, whenever hardware 
		virtualization stops (called a "vmexit"), such as to emulate some virtual hardware, 
		the kvm_run structure will contain information about why it stopped. We map this structure 
		into user space using mmap(), but first, we need to know how much memory to map, 
		which KVM tells us with the KVM_GET_VCPU_MMAP_SIZE ioctl():
	 */

		/*Besides the guest memory, every VCPU also allocates a small portion of VCPU runtime 
	  memory from the host OS in the function , to store the information it has to exchange with KVM.
	   In which lines of the program is this memory allocated, what is its size?  = vcpu_mmap_size */
 	 
	vcpu_mmap_size = ioctl(vm->sys_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
        if (vcpu_mmap_size <= 0) {
		perror("KVM_GET_VCPU_MMAP_SIZE");
                exit(1);
	}

	/*and where is it located in the virutal address space of the hypervisor?*/
	vcpu->kvm_run = mmap(NULL, vcpu_mmap_size, PROT_READ | PROT_WRITE,
			     MAP_SHARED, vcpu->fd, 0);
	if (vcpu->kvm_run == MAP_FAILED) {
		perror("mmap kvm_run");
		exit(1);
	}
}


int run_vm(struct vm *vm, struct vcpu *vcpu, size_t sz)
{
	struct kvm_regs regs;
	uint64_t memval = 0;
	int count=0;
	int file_descp =0;
	uint32_t *file_info;

	for (;;) {



		if (ioctl(vcpu->fd, KVM_RUN, 0) < 0) {
			perror("KVM_RUN");
			exit(1);
		}
		/*
		At which line does the control switch back to the hypervisor from the guest?
		*/
		switch (vcpu->kvm_run->exit_reason) {
		case KVM_EXIT_HLT:
			goto check;

		case KVM_EXIT_IO:
			count = count+1;
			switch(vcpu->kvm_run->io.port)
			{
				/***For character I/O***/
				case 0xE9:
					if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_OUT)
					{
						char *p = (char *)vcpu->kvm_run;
						//Data offset contains io data 
						fwrite(p + vcpu->kvm_run->io.data_offset,
				       		vcpu->kvm_run->io.size, 1, stdout);
						fflush(stdout);
						continue;
					}
					if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_IN)
					{
						char *s = (char *)vcpu->kvm_run;
						int *val = (int*)(s + vcpu->kvm_run->io.data_offset);
						*val = 0x21;
						continue;
					}
					break;
				/*** For integer I/O **/
				case 0xEA:
					if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_OUT)
					{
						char *p = (char *)vcpu->kvm_run;
						//Data offset contains io data 
						printf("Value = %d \n",*(p + vcpu->kvm_run->io.data_offset));
						continue;
					}
					if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_IN)
					{
						char *s = (char *)vcpu->kvm_run;
						int *val = (int*)(s + vcpu->kvm_run->io.data_offset);
						/*** Pass the value from hypervisor here ***/
						*val = 0x21; 
						continue;
					}
				break;
				/*** Return number of exit calls to guest ***/
				case 0xEB:
					if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_IN)
					{
						char *s = (char *)vcpu->kvm_run;
						int *val = (int*)(s + vcpu->kvm_run->io.data_offset);
						/*** Pass the value from hypervisor here ***/
						*val = count; 
						continue;
					}
				break;
				/*** Print entire string using just one call ***/
				case 0xEC:
					if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_OUT)
					{
						char *p = (char *)vcpu->kvm_run;
						//char *val = (char*)(p + vcpu->kvm_run->io.data_offset);
						//Data offset contains io data 
						//printf("Value = %d \n",((vm->mem)+val));
						uint32_t addr = *(p + vcpu->kvm_run->io.data_offset);
						//int addro = *(p + vcpu->kvm_run->io.data_offset);
						printf("The string is : %s \n",&(vm->mem[addr&EXTRACT_GUEST]));
						continue;
					}
				break;
				/*** Open file call ***/
				case 0xED:
					if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_OUT)
					{
						char *p = (char *)vcpu->kvm_run;
						uint32_t addr = *(p + vcpu->kvm_run->io.data_offset);
						char *file = &(vm->mem[addr&EXTRACT_GUEST]);
						printf("File name obtained = %s\n",file);
						file_descp = open(file, O_CREAT | O_RDWR ,0777);
						printf("File descriptor of opened file = %d\n",file_descp);
						continue;
					}
					if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_IN)
					{
						char *s = (char *)vcpu->kvm_run;
						int *val = (int*)(s + vcpu->kvm_run->io.data_offset);
						/*** Pass the value of file desciptor from hypervisor here ***/
						*val = file_descp; 
						continue;
					}
				break;

				/** File read call **/
				case 0xEE:
					if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_OUT)
					{
						char *p = (char *)vcpu->kvm_run;
						uint32_t addr = *(p + vcpu->kvm_run->io.data_offset);
						file_info = ((uint32_t*)&(vm->mem[addr&EXTRACT_GUEST]));
						//char *s = file_info;
						//printf("%s\n",s);
						// printf("Read call : fd = %x, Num  bytes = %d\n", file_info[0], file_info[2]);
						// printf("addr = 0x%x \n",addr);
						// printf("%s %c \n", (file_info),(file_info[1]));
						//printf("strng print op : %d\n",*((uint32_t*)file_info[0]));
						int numbytes = read(file_info[0], &vm->mem[file_info[1]&EXTRACT_GUEST], file_info[2]);
						printf("The string from file is = %s \n Num of bytes read = %d\n", &vm->mem[file_info[1]&EXTRACT_GUEST], numbytes);
						//close(file_info[0]);
						continue;
					}
				break;

				/** File write call **/
				case 0xEF:
					if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_OUT)
					{
						char *p = (char *)vcpu->kvm_run;
						uint32_t addr = *(p + vcpu->kvm_run->io.data_offset);
						file_info = ((uint32_t*)&(vm->mem[addr&EXTRACT_GUEST]));
						printf("Write code starts here \n");
						printf("Write call : fd = %d, string = %s, bytes = %d\n", file_info[0],&vm->mem[file_info[1]&EXTRACT_GUEST], file_info[2]);
						printf("addr = 0x%x \n",addr);
                        int ret = write(file_info[0], &vm->mem[file_info[1]&EXTRACT_GUEST], file_info[2]);
						printf ("write returned = %d",ret);
						continue;
					}
				break;


				/** File seek call **/
				case 0xF0:
					if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_OUT)
					{
						char *p = (char *)vcpu->kvm_run;
						uint32_t addr = *(p + vcpu->kvm_run->io.data_offset);
						file_info = ((uint32_t*)&(vm->mem[addr&EXTRACT_GUEST]));
                        printf("\n seek call : fd = %d, offset = %d\n", file_info[0], file_info[1]);
                        lseek(file_info[0], file_info[1], SEEK_SET);
						continue;
					}
				break;
				default:
					break;
			}

			/* fall through */
		default:
			fprintf(stderr,	"Got exit_reason %d,"
				" expected KVM_EXIT_HLT (%d)\n",
				vcpu->kvm_run->exit_reason, KVM_EXIT_HLT);
			exit(1);
		}
	}



 check:
	if (ioctl(vcpu->fd, KVM_GET_REGS, &regs) < 0) {
		perror("KVM_GET_REGS");
		exit(1);
	}

	if (regs.rax != 42) {
		printf("Wrong result: {E,R,}AX is %lld\n", regs.rax);
		return 0;
	}

	memcpy(&memval, &vm->mem[0x400], sz);

	if (memval != 42) {
		printf("Wrong result: memory at 0x400 is %lld\n",
		       (unsigned long long)memval);
		return 0;
	}
	printf("Total number of times EXIT IO called = %d \n",count);
	return 1;
}

extern const unsigned char guest16[], guest16_end[];

int run_real_mode(struct vm *vm, struct vcpu *vcpu)
{
	struct kvm_sregs sregs;
	struct kvm_regs regs;

	printf("Testing real mode\n");

        if (ioctl(vcpu->fd, KVM_GET_SREGS, &sregs) < 0) {
		perror("KVM_GET_SREGS");
		exit(1);
	}

	sregs.cs.selector = 0;
	sregs.cs.base = 0;

        if (ioctl(vcpu->fd, KVM_SET_SREGS, &sregs) < 0) {
		perror("KVM_SET_SREGS");
		exit(1);
	}

	memset(&regs, 0, sizeof(regs));
	/* Clear all FLAGS bits, except bit 1 which is always set. */
	regs.rflags = 2;
	regs.rip = 0;

	if (ioctl(vcpu->fd, KVM_SET_REGS, &regs) < 0) {
		perror("KVM_SET_REGS");
		exit(1);
	}

	memcpy(vm->mem, guest16, guest16_end-guest16);
	return run_vm(vm, vcpu, 2);
}

static void setup_protected_mode(struct kvm_sregs *sregs)
{
	struct kvm_segment seg = {
		.base = 0,
		.limit = 0xffffffff,
		.selector = 1 << 3,
		.present = 1,
		.type = 11, /* Code: execute, read, accessed */
		.dpl = 0,
		.db = 1,
		.s = 1, /* Code/data */
		.l = 0,
		.g = 1, /* 4KB granularity */
	};

	sregs->cr0 |= CR0_PE; /* enter protected mode */

	sregs->cs = seg;

	seg.type = 3; /* Data: read/write, accessed */
	seg.selector = 2 << 3;
	sregs->ds = sregs->es = sregs->fs = sregs->gs = sregs->ss = seg;
}

extern const unsigned char guest32[], guest32_end[];

int run_protected_mode(struct vm *vm, struct vcpu *vcpu)
{
	struct kvm_sregs sregs;
	struct kvm_regs regs;

	printf("Testing protected mode\n");

        if (ioctl(vcpu->fd, KVM_GET_SREGS, &sregs) < 0) {
		perror("KVM_GET_SREGS");
		exit(1);
	}

	setup_protected_mode(&sregs);

        if (ioctl(vcpu->fd, KVM_SET_SREGS, &sregs) < 0) {
		perror("KVM_SET_SREGS");
		exit(1);
	}

	memset(&regs, 0, sizeof(regs));
	/* Clear all FLAGS bits, except bit 1 which is always set. */
	regs.rflags = 2;
	regs.rip = 0;

	if (ioctl(vcpu->fd, KVM_SET_REGS, &regs) < 0) {
		perror("KVM_SET_REGS");
		exit(1);
	}

	memcpy(vm->mem, guest32, guest32_end-guest32);
	return run_vm(vm, vcpu, 4);
}

static void setup_paged_32bit_mode(struct vm *vm, struct kvm_sregs *sregs)
{
	uint32_t pd_addr = 0x2000;
	uint32_t *pd = (void *)(vm->mem + pd_addr);

	/* A single 4MB page to cover the memory region */
	pd[0] = PDE32_PRESENT | PDE32_RW | PDE32_USER | PDE32_PS;
	/* Other PDEs are left zeroed, meaning not present. */

	sregs->cr3 = pd_addr;
	sregs->cr4 = CR4_PSE;
	sregs->cr0
		= CR0_PE | CR0_MP | CR0_ET | CR0_NE | CR0_WP | CR0_AM | CR0_PG;
	sregs->efer = 0;
}

int run_paged_32bit_mode(struct vm *vm, struct vcpu *vcpu)
{
	struct kvm_sregs sregs;
	struct kvm_regs regs;

	printf("Testing 32-bit paging\n");

        if (ioctl(vcpu->fd, KVM_GET_SREGS, &sregs) < 0) {
		perror("KVM_GET_SREGS");
		exit(1);
	}

	setup_protected_mode(&sregs);
	setup_paged_32bit_mode(vm, &sregs);

        if (ioctl(vcpu->fd, KVM_SET_SREGS, &sregs) < 0) {
		perror("KVM_SET_SREGS");
		exit(1);
	}

	memset(&regs, 0, sizeof(regs));
	/* Clear all FLAGS bits, except bit 1 which is always set. */
	regs.rflags = 2;
	regs.rip = 0;

	if (ioctl(vcpu->fd, KVM_SET_REGS, &regs) < 0) {
		perror("KVM_SET_REGS");
		exit(1);
	}

	memcpy(vm->mem, guest32, guest32_end-guest32);
	return run_vm(vm, vcpu, 4);
}

extern const unsigned char guest64[], guest64_end[];

static void setup_64bit_code_segment(struct kvm_sregs *sregs)
{
	struct kvm_segment seg = {
		.base = 0,
		.limit = 0xffffffff,
		.selector = 1 << 3,
		.present = 1,
		.type = 11, /* Code: execute, read, accessed */
		.dpl = 0,
		.db = 0,
		.s = 1, /* Code/data */
		.l = 1,
		.g = 1, /* 4KB granularity */
	};

	sregs->cs = seg;

	seg.type = 3; /* Data: read/write, accessed */
	seg.selector = 2 << 3;
	sregs->ds = sregs->es = sregs->fs = sregs->gs = sregs->ss = seg;
}

static void setup_long_mode(struct vm *vm, struct kvm_sregs *sregs)
{
	/*
	Before we can run code, we need to set up the initial 
	states of these sets of registers. Of the "special" registers (sregs), we only need to 
	change the code segment (cs); its default state (along with the initial instruction pointer) 
	points to the reset vector at 16 bytes below the top of memory, but we want cs to point to 0 
	instead. Each segment in kvm_sregs includes a full segment descriptor; we don't need to change 
	the various flags or the limit, but we zero the base and selector fields which together 
	determine what address in memory the segment points to. To avoid changing any of the other 
	initial "special" register states, we read them out, change cs, and write them back:

	*/

	/*

	In the x86-64 computer architecture, long mode is the mode where a 64-bit operating system can 
	access 64-bit instructions and registers. 64-bit programs are run in a sub-mode called 64-bit mode, 
	while 32-bit programs and 16-bit protected mode programs are executed in a sub-mode called 
	compatibility mode.Real mode or virtual 8086 mode programs cannot be natively run in long mode.
	*/


	//Page table has 4 levels

	uint64_t pml4_addr = 0x2000; //Page map level 4
	uint64_t *pml4 = (void *)(vm->mem + pml4_addr);

	uint64_t pdpt_addr = 0x3000;  //Page directory pointer table
	uint64_t *pdpt = (void *)(vm->mem + pdpt_addr);

	uint64_t pd_addr = 0x4000; //Page directory table
	uint64_t *pd = (void *)(vm->mem + pd_addr);

	pml4[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | pdpt_addr;
	pdpt[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | pd_addr;
	pd[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | PDE64_PS;

	sregs->cr3 = pml4_addr;
	sregs->cr4 = CR4_PAE; 
	// when phsical address extension is enabled via the CR4_PAE flag, 
	//the page size is treated as 2MB, and the last 21 bits are used to index into the pgdir. 

	sregs->cr0
		= CR0_PE | CR0_MP | CR0_ET | CR0_NE | CR0_WP | CR0_AM | CR0_PG;
	sregs->efer = EFER_LME | EFER_LMA;

	//To enable syscall (for file handling) ,setting up 
	sregs->efer |= EFER_SCE ; 


	setup_64bit_code_segment(sregs);
}

int run_long_mode(struct vm *vm, struct vcpu *vcpu)
{
	struct kvm_sregs sregs;
	struct kvm_regs regs;


	/*
	To avoid changing any of the other 
	initial "special" register states, we read them out, change cs, and write them back:

	*/
	printf("Testing 64-bit mode\n");
	printf("This is the LONG MODE\n");

        if (ioctl(vcpu->fd, KVM_GET_SREGS, &sregs) < 0) {
		perror("KVM_GET_SREGS");
		exit(1);
	}

	setup_long_mode(vm, &sregs);

        if (ioctl(vcpu->fd, KVM_SET_SREGS, &sregs) < 0) {
		perror("KVM_SET_SREGS");
		exit(1);
	}
	/*For the standard registers, we set most of them to 0, 
	other than our initial instruction pointer , flags and stack pointer*/

	memset(&regs, 0, sizeof(regs));
	/* Clear all FLAGS bits, except bit 1 which is always set. */
	regs.rflags = 2;
	regs.rip = 0;
	/* Create stack at top of 2 MB page and grow down. */
	regs.rsp = 2 << 20;

	if (ioctl(vcpu->fd, KVM_SET_REGS, &regs) < 0) {
		perror("KVM_SET_REGS");
		exit(1);
	}
	/*At which line in the hypervisor program does the control switch 
	from running the hypervisor to running the guest? */

	/*
	need to copy our machine code into memory of vm
	 At what (guest virtual) address does the guest start execution when it runs? 
	 Where is this address configured?
	*/
	memcpy(vm->mem, guest64, guest64_end-guest64);

	return run_vm(vm, vcpu, 8);
}


int main(int argc, char **argv)
{
	struct vm vm;
	struct vcpu vcpu;
	enum {
		REAL_MODE,
		PROTECTED_MODE,
		PAGED_32BIT_MODE,
		LONG_MODE,
	} mode = REAL_MODE;
	int opt;

	while ((opt = getopt(argc, argv, "rspl")) != -1) {
		switch (opt) {
		case 'r':
			mode = REAL_MODE;
			break;

		case 's':
			mode = PROTECTED_MODE;
			break;

		case 'p':
			mode = PAGED_32BIT_MODE;
			break;

		case 'l':
			mode = LONG_MODE;
			break;

		default:
			fprintf(stderr, "Usage: %s [ -r | -s | -p | -l ]\n",
				argv[0]);
			return 1;
		}
	}

	/* 2 MB*/

	vm_init(&vm, 0x200000); 
	vcpu_init(&vm, &vcpu);

	switch (mode) {
	case REAL_MODE:
		return !run_real_mode(&vm, &vcpu);

	case PROTECTED_MODE:
		return !run_protected_mode(&vm, &vcpu);

	case PAGED_32BIT_MODE:
		return !run_paged_32bit_mode(&vm, &vcpu);

	case LONG_MODE:
		return !run_long_mode(&vm, &vcpu);
	}

	return 1;
}
