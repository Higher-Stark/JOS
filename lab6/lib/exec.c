// implement exec from user space

#include <inc/string.h>
#include <inc/lib.h>
#include <inc/elf.h>

#define EXEC_TEMP 0xE0000000

//
// User-level exec
// open the file, ELF expected.
// Load each segment expected loaded into EXEC_TEMP space.
// Call sys_exec to handle the rest.
int exec(const char *path, const char **argv)
{
  int fd, i, r;
  if ((fd = open(path, O_RDONLY)) < 0) {
    cprintf("exec: file %s not found\n", path);
    return fd;
  }
  struct Stat stat;
  if ((r = fstat(fd, &stat)) < 0) {
    cprintf("exec: fstat error %e", r);
    close(fd);
    return r;
  }

  char *elf_buf = (char *)EXEC_TEMP;

  struct Elf *elf;
  elf = (struct Elf*)elf_buf;

  // Read file and store at 0xE0000000
  // Then check the file is executable
  ssize_t left = stat.st_size;
  for (uintptr_t va = EXEC_TEMP; va < ROUNDUP(EXEC_TEMP + stat.st_size, PGSIZE); va += PGSIZE) {
    if ((r = sys_page_alloc(0, (void *)va, PTE_U | PTE_W)) < 0) {
      cprintf("exec: page allocation error %e\n", r);
      close(fd);
      return r;
    }
    ssize_t toread = MIN(left, PGSIZE);
    if ((readn(fd, (void *)va, toread) != toread)) {
      cprintf("read didn't return expected bytes.\n");
    }
    left -= toread;
  }
  if (elf->e_magic != ELF_MAGIC) {
    close(fd);
    cprintf("elf magic %08x want %08x\n", elf->e_magic, ELF_MAGIC);
    return -E_NOT_EXEC;
  }

  close(fd);
  fd = -1;

  // Call sys_exec to run the file in this curenv
  r = sys_exec(elf, argv);
  
  // We are not expected be reach here unless error in sys_exec
  if (r < 0) {
    cprintf("exec %s failed\n", path);
    return r;
  }
  // Indeed, never should program reach here.
  return 0;
}

// Exec, taking command-line arguments array directly on the stack.
// NOTE: Must have a sequential of NULL at the end of the args
// (none of the args may be NULL).
int execl(const char *path, const char *arg0, ...)
{
  int argc = 0;
  va_list vl;
  va_start(vl, arg0);
  while (va_arg(vl, void *) != NULL)
    argc++;
  va_end(vl);

  const char *argv[argc + 2];
  argv[0] = arg0;
  argv[argc + 1] = NULL;

  va_start(vl, arg0);
  unsigned i;
  for (i = 0; i < argc; i++)
    argv[i + 1] = va_arg(vl, const char *);
  va_end(vl);
  return exec(path, argv);
}