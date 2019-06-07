# Lab 6 notes

## Exercises
### Exercise 1
The variable `static unsigned int ticks` is not shared among CPUs, each CPU has a copy of its own. `ticks` is like a local variable in thread.

When time interrupt comes, before calling `sched_yield()`, we should call `time_tick()` as `sched_yield()` is a non-return function and nothing else.

### Question 1

My output server is listening for IPC all the time.
For each IPC, the output server puts the data to NIC by calling `sys_net_send()`. 
As we assume the largest Ethernet packet is 1518 bytes, we don't care about the fragment. We just make sure that the environment has read permisson to the buffer. Then we just call `e1000_tx()` to send it to hardware.
__If the queue is full, we will return `-E_AGAIN` back to output.  The output will retry for 3 times at most and move on to next one.__ 
In `e1000_tx()`, we will check the packet length, the transmission ring, and then add the packet into the ring.
As `EOP` stands for end of packet, we mark it up for the every packet.

### Question 2

Receive descriptor queue structure

RDH is manipulated by _hardware_. When a packet is received, if the queue is not empty, the hardware add it into the queue and then increase the head pointer.

RDT is manipulated by _software_. When software wants to get a packet, if the next packet after RDT pointer has DD bit set in status, just return the next packet and increase the RDT pointer.

If RDH is same with RDT, the queue is empty(full is more easier to understand? no available descriptor to store the incoming packet).

In the document, the RDH and RDT is initiated to 0b by hardware. Thus we need to set RDT to `N_RXDESC - 1` or set RDH to 1.

For `e1000_rx()`, we check the next packet's DD bit in status. If set, we return the packet. 
__As document says, read RDH is not realiable. Checking DD bit is better option.__

* If receive queue is empty, we just return `-E_AGAIN`, and let the upper layer to decide.

we'd better to clear the status as document perfers.

For `sys_net_recv()`, first check the permission to `[buf - buf + len]` space. Then just call `e1000_rx()`. Whatever we get, we just return to upper layer even `E_AGAIN`. 
__This is quite important not to handle `E_AGAIN` in kernel. In JOS only one environment can obtain lock and enter kernel mode. And interrupt is disabled in kernel mode. Retry `e1000_rx()` preventing other environment to send.__

In `input()`, we keep retrying until we get a packet. Before receiption, we should allocate a page for `struct Nsipc nsipcbuf`. Once we get the packet, send it out through IPC. 
For netserver to have time to copy the packet, we call `yield()` for a couple of times after IPC.