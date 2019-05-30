// hello, world
#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	cprintf("hello, world\n");
	cprintf("i am environment %08x\n", thisenv->env_id);

	// Test exec
	char ss[2][30] = {
		"/echo",
		"I am environment -_-"
	};

	char *vs[3] = {ss[0], ss[1], 0};
	int r = exec("/echo", (const char **)vs);
	cprintf("Not expected #_# here [%4d] !\n", r);
}
