#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int sys_fork(void)
{
  return fork();
}

int sys_exit(void)
{
  exit();
  return 0; // not reached
}

int sys_wait(void)
{
  return wait();
}

int sys_kill(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int sys_getpid(void)
{
  return myproc()->pid;
}

int sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

int sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (myproc()->killed)
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

extern int list_all_processes(void);
int sys_list_all_processes(void)
{
  cprintf("Enter kernel\n");
  list_all_processes();
  return 0;
}

int sys_create_palindrome(void)
{
  int n = myproc()->tf->ebx;
  create_palindrome(n);
  return 0;
}

int sys_sort_syscalls(void)
{
  int pid;
  if (argint(0, &pid) < 0)
    return -1;

  struct proc *p = findproc(pid);
  if (!p) // Process not found
  {
    cprintf("Process not found!\n");
    return -1;
  }
  // Sort system calls for this process
  sort_syscalls(p->syscalls, p->unique_syscalls_count);

  // Print the sorted system calls
  for (int i = 0; i < p->unique_syscalls_count; i++)
  {
    cprintf("Syscall %d\n", p->syscalls[i]);
  }
  return 0;
}

int sys_get_most_invoked_syscall(void)
{
  int pid;
  if (argint(0, &pid) < 0)
  {
    cprintf("Invalid input\n");
    return -1;
  }
  int result = get_most_invoked_syscall(pid);
  return result;
}
