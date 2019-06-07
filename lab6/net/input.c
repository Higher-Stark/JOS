#include "ns.h"

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server (using ipc_send with NSREQ_INPUT as value)
	//	do the above things in a loop
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
	int r = 0;

	// struct jif_pkt *buf = (struct jif_pkt *)REQVA;
	// cprintf("\033[34m[INFO]\033[0m &nsipcbuf: %08p\n", &nsipcbuf);
	// cprintf("\033[34m[INFO]\033[0m buf: %08p\n", buf);
	if ((r = sys_page_alloc(0, &nsipcbuf, PTE_U | PTE_W)) < 0) 
	 	panic("\033[31m[ERROR]\033[0m sys_page_alloc(): %e", r);

	
	while (true) {
		while ((r = sys_net_recv((void *)nsipcbuf.pkt.jp_data, 0)) == -E_AGAIN)
			sys_yield();
		if (r < 0)
			panic("input: \033[31m[ERROR]\033[0m %e", r);

		nsipcbuf.pkt.jp_len = r;
		ipc_send(ns_envid, NSREQ_INPUT, (void *)&nsipcbuf, PTE_U | PTE_P | PTE_W);

		for (int i = 0; i < 3; ++i) sys_yield();
	}
}
