// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display a backtrace of the function stack", mon_backtrace },
	{ "time", "time kerninfo",mon_time },
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

// Lab1 only
// read the pointer to the retaddr on the stack
static uint32_t
read_pretaddr() {
    uint32_t pretaddr;
    __asm __volatile("leal 4(%%ebp), %0" : "=r" (pretaddr)); 
    return pretaddr;
}

void
do_overflow(void)
{
    cprintf("Overflow success\n");
}


void
start_overflow(void)
{
	// You should use a techique similar to buffer overflow
	// to invoke the do_overflow function and
	// the procedure must return normally.

	// And you must use the "cprintf" function with %n specifier
	// you augmented in the "Exercise 9" to do this job.

	// hint: You can use the read_pretaddr function to retrieve 
	//       the pointer to the function call return address;

	char str[256] = {};
	int nstr = 0;
	char *pret_addr;

	// Your code here.
	pret_addr = (char *)read_pretaddr();
	uint32_t maladdr = (uint32_t)do_overflow;
	for (int i = 0; i != 4; i++) {
		cprintf("%*s%n", pret_addr[i] & 0xff, "\x0d", pret_addr + 4 + i);
	}
	for (int i = 0; i != 4; i++) {
		cprintf("%*s%n", (maladdr >> (i * 8)) & 0xff, "\x0d", pret_addr + i);
	}
}


int mon_time(int argc, char **argv, struct Trapframe *tf)
{
	if (argc != 2)
		return -1;

	uint64_t before, after;
	int i;
	struct Command command;
	/* search */
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(commands[i].name, argv[1]) == 0) {
				break;
		}
	}
	if (i == ARRAY_SIZE(commands))
		return -1;
	/* run */
	before = read_tsc();
	(commands[i].func)(1, argv+1, tf);
	after = read_tsc();
	cprintf("%s cycles: %d\n", commands[i].name, after - before);
	return 0;
}

void
overflow_me(void)
{
        start_overflow();
}


int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	unsigned int *ebp = (unsigned int*)read_ebp();
	struct Eipdebuginfo debug_info;
	unsigned int eip = ebp[1]; // Upper level eip.
	while (ebp != NULL) {
		cprintf("  eip %08x  ebp %08x  args %08x %08x %08x %08x %08x\n", eip, ebp, ebp[2], ebp[3], ebp[4], ebp[5], ebp[6]);
		debuginfo_eip(eip, &debug_info);
		cprintf("	 %s:%d ", debug_info.eip_file, debug_info.eip_line);
		for(int i = 0; i < debug_info.eip_fn_namelen; i++) 
			cprintf("%c", debug_info.eip_fn_name[i]);
		cprintf("+%d\n", eip - debug_info.eip_fn_addr);
		ebp = (unsigned int *)ebp[0]; 
		eip = ebp[1]; 
	}

	//overflow_me();
    	cprintf("Backtrace success\n");
	cprintf("Overflow success\n");
	return 0;
}




/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
