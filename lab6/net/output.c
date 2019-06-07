#include "ns.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet request (using ipc_recv)
	//	- send the packet to the device driver (using sys_net_send)
	//	do the above things in a loop
	while (true) {
		void *pg = 0;
		int perm = 0;
		int r = ipc_recv(&ns_envid, &nsipcbuf, &perm);
		if (r < 0) 
			cprintf("output: \033[31m[ERROR]\033[0m ipc_recv() %e\n", r);

		int retry = 0;

		while (retry < 3) {
			r = sys_net_send(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len);
			if (r == nsipcbuf.pkt.jp_len)
				break;
			if (r == -E_INVAL) {
				cprintf("output: invalid length %d\n", nsipcbuf.pkt.jp_len);
				break;
			}
			retry++;
		}

		if (retry == 3)
			// r = sys_net_send("\0\0\0\0", 4);
			cprintf("output: \033[32m[WARN]\033[0m packet not fully transmitted for 3 times, abandon\n");
	}
}
