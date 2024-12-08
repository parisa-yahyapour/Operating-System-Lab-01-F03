#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
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
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

extern int change_queue(int pid, int new_queue);


int sys_change_scheduling_queue(void) {
    int queue_number, pid;
   
    // Get arguments from the user space
    if (argint(0, &pid) < 0 || argint(1, &queue_number) < 0)
        return -1;
    // Call the function to change the queue
    int a=change_queue(pid, queue_number);
    return a;
}

int sys_set_process_parameter(void)
{
  int pid, confidence, time_burst;
  if (argint(0, &pid) < 0 || argint(1, &time_burst) < 0 || argint(2, &confidence) < 0)
  {
    return -1;
  }
  set_process_parameter(pid, time_burst, confidence);
  //cprintf("hi\n");
  return 0;
}

extern void print_process_info(void);

int sys_print_process_info(void)
{
  print_process_info();
  return 0;
}