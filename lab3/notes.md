# Lab 3 notes

## Lab 内容简介

Lab 3 会在系统启动后创建第一个用户态进程，本次lab只会有一个用户态进程，后续的lab会产生多个用户太进程。

JOS的用户态进程称为一个environment，与xv6的process有所不同。
JOS下只会有一个活跃的environment,所以不是每个env都会有一个Trap frame。JOS中只有一个trap frame。

多任务、不同的等级要求我们记录每个env，从而实现对envs的管理。主要有以下几点：

1. env的创建
2. env的执行
3. env的终结
4. env运行中系统调用与异常

这一些点会牵涉到每个env内存表的构造、内存的分配映射，程序文件的加载，trap frame的使用等。

_本次lab会涉及一些gcc的内联汇编代码，但对本次lab并不一定需要使用到_

## Way Through the Lab
### Part A
#### Exercise 1

JOS 内核维护了整个系统的environments，通过
```C
struct Env *envs = NULL;		// all environments
struct Env *curenv = NULL;		// current env
static struct Env *env_free_list;	// Free environment list
```
三个变量。`envs`是系统中所有environments所在的数组，分配空间在执行`page_init()`之前，使用`boot_alloc()`分配数组内存。
`env_free_list`要求第一个被分配的environment是`envs`数组的第一个，将`env_free_list`指向`envs`即可。`env_free_list`是个链表结构，只需要在初始化envs数组时将前一个指向后一个就可以了。

#### Exercise 2

Lab 文档中提到 JOS 的内核内嵌了一些用户态的程序，以二进制的ELF格式。
这样就可以直接从内存中复制，而不需要像`main.c`中的`bootmain()`函数一样从磁盘读取。

`env.c`中需要我们实现的有`env_init()`, `env_setup_vm()`, `region_alloc()`, `load_icode()`, `env_create()`, `env_run()`这几个函数。

* `env_init()`函数调用发生在`mem_init()`之后，负责初始化`envs`这一数组和`env_free_list`。
  同时还要对每个CPU进行部分初始化。
* `env_setup_vm()`函数完成对`env` `UTOP`以上的虚拟内存的映射。
  因为对于所有的environments来说，初始化时`UTOP`以上的虚拟地址空间都是一样的（除了`UVPT`所在的那一个page以外），初始化时只需要将内核的page directory tabl复制过去即可。
  User env的page directory table所在的页需要手动管理它的reference counter，`page_free()`才能正常地free这一page。
* `region_alloc()`这个函数分配`len`字节的物理内存空间，并映射到environment的`va`虚拟地址空间。  
  但真实情况下物理内存的分配都是以page为最小单位，所以需要对`va`向下对齐，对`va + len`向上对齐，再从物理内存中分配相应大小的资源。
* `load_icode`函数将一个二进制可执行文件加载到一个environment中，首先检查二进制文件是否是ELF格式的可执行文件，再接着读取ELF文件的头部，将ELF文件的`.text`, `.data`等部分通过`region_alloc()`加载到相应的内存地址。  
  因为ELF的`.text`等部分应加载到environment对应的内存空间，所以在复制之前需要将`CR3`寄存器变成environment的page directory的物理地址。在复制完成后恢复到kernel的page directory的物理地址。
* `env_create()`根据给定的二进制文件创建一个environment，并设置其类型为给定的`type`。
* `env_run()`的发生意味着context switch，如果`curenv`非空且处于正在运行状态，就将其状态置为可运行。将`curenv`改为新的environment，再把新的environment标为正在运行，切换`CR3`，最后需要`env_pop_tf()`将trap frame中的状态恢复。