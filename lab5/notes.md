# Lab 5 note

## Brief about lab 5
### Spawn

`spawn` is a library call to load and run on-disk executables.

We are expected to implement `spawn` in this lab working with a file system. 
And we are supposed to run a shell by fleshing out kernel and library operating system.

## Solution
### File system

The file system to work with is much simpler than most real file system, including that of xv6 UNIX.

- [ ] multi-user support (file ownership, permissions)
- [ ] hard links
- [ ] symbolic links
- [ ] time stamps
- [ ] special devices

### On-Disk File System Structure

In most UNIX file systems, file's most meta data is stored in inode, while file name is stored in directory entries. The name inode mapping in directory entries make it possible to implement hard links and symbolic links.

But JOS doesn't support many features listed above, so we simply **store file's meta-data in the directory entry**.

The JOS allow user environment to _read_ directory meta-data directly. Thus user environment is able to implement `ls` independent on file system call.
### Disk Access
In previous design, JOS only has one trap frame for each core. Once the core receives an interrupt, handler is invoked. The interrupt doesn't specify the target environment.
In that case, disk interrupt doesn't work that putting the environment doing disk access to sleep and wake it up when disk interrupt occurs.

JOS relies on polling to implement disk access in user space. But not all environment is allowed to execute special device I/O instructions. The **IOPL bits** in `EFLAGS` register determines whether the environment is priviledged to perform device I/O instructions.
### Exercise 1
JOS only allows file system environments to perform special I/O instructions. In `env_create()`, we should set `IOPL bits` in `env->trap frame.eflags`.

### Question 1
Context switch will store the old `EFLAGS` in trap frame and restore it when continue. `IOPL bits` are not likely to be changed once the environment is created. There is nothing to worry for storing and restoring I/O privilege settings.

### Block cache
Block cache is a part of memory space corresponding to the disk blocks. The JOS supports at most 3GB disks, and the block cache is just a 3GB memory space mapping.

### Exercise 2
`bg_pgfault()` is triggered when a block is not mapped from disk to memory.
So first allocate a page for the block and read the block from the disk. Before return, clear the dirty bit.

`flush_block()` flush a mapped and dirty block from memory to disk. After writing to disk, clear the dirty bit.

### The Block Bitmap
The block bitmap records whether a block is used, 0 for used while 1 for free.

### File Operations
`file_block_walk()` finds the pointer to n th data block in `File`. The n th data block is in direct list or indirect list. 
If in indirect list but indirect block is not allocated, allocate a block if in need and set the indirect field in `File`. 

`file_get_block()` get the block number of the n th data block in `File`. If block number is 0, no block is allocated. Allocate a block and put the block number into `File`.

### The File system interface
In JOS, there is a file system environment in charge of file system as the agent for other environment. 
Other environment accesses file system by send IPC to that environment.

### Exercise 5
Reading a file in `serve_read()`, we first look up the open file in current environment. 
Then we read request bytes from the file at the offset (the offset lies in file descriptor) and put into destination buffer.

### Exercise 6
Writing a file in `serve_write()`, the routine is quite similiar to `serve_read()`.

For `devfile_write()`, constructing IPC arguments is similiar to `devfile_read()`. The most significant point is that `fsipcbuf.write.req_buf` is an array of char, we should copy requested bytes from `buf` to `req_buf`.