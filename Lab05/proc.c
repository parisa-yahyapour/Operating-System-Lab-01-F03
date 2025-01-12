#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"

struct
{
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int cpuid()
{
  return mycpu() - cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu *
mycpu(void)
{
  int apicid, i;

  if (readeflags() & FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i)
  {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc *
myproc(void)
{
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

// PAGEBREAK: 32
//  Look in the process table for an UNUSED proc.
//  If found, change state to EMBRYO and initialize
//  state required to run in the kernel.
//  Otherwise return 0.
static struct proc *
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if ((p->kstack = kalloc()) == 0)
  {
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe *)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint *)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context *)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;
  p->top = 0xA0000; // lab 5
  return p;
}

// PAGEBREAK: 32
//  Set up first user process.
void userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();

  initproc = p;
  if ((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0; // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if (n > 0)
  {
    if ((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  else if (n < 0)
  {
    if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0)
  {
    return -1;
  }

  // Copy process state from proc.
  if ((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0)
  {
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for (i = 0; i < NOFILE; i++)
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if (curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for (fd = 0; fd < NOFILE; fd++)
  {
    if (curproc->ofile[fd])
    {
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->parent == curproc)
    {
      p->parent = initproc;
      if (p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->parent != curproc)
        continue;
      havekids = 1;
      if (p->state == ZOMBIE)
      {
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || curproc->killed)
    {
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock); // DOC: wait-sleep
  }
}

// PAGEBREAK: 42
//  Per-CPU process scheduler.
//  Each CPU calls scheduler() after setting itself up.
//  Scheduler never returns.  It loops, doing:
//   - choose a process to run
//   - swtch to start running that process
//   - eventually that process transfers control
//       via swtch back to the scheduler.
void scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;

  for (;;)
  {
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void)
{
  int intena;
  struct proc *p = myproc();

  if (!holding(&ptable.lock))
    panic("sched ptable.lock");
  if (mycpu()->ncli != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched running");
  if (readeflags() & FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
  acquire(&ptable.lock); // DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first)
  {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if (p == 0)
    panic("sleep");

  if (lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if (lk != &ptable.lock)
  {                        // DOC: sleeplock0
    acquire(&ptable.lock); // DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if (lk != &ptable.lock)
  { // DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

// PAGEBREAK!
//  Wake up all processes sleeping on chan.
//  The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      p->killed = 1;
      // Wake process from sleep if necessary.
      if (p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// PAGEBREAK: 36
//  Print a process listing to console.  For debugging.
//  Runs when user types ^P on console.
//  No lock to avoid wedging a stuck machine further.
void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      [EMBRYO] "embryo",
      [SLEEPING] "sleep ",
      [RUNNABLE] "runble",
      [RUNNING] "run   ",
      [ZOMBIE] "zombie"};
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if (p->state == SLEEPING)
    {
      getcallerpcs((uint *)p->context->ebp + 2, pc);
      for (i = 0; i < 10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
// lab 5
#define MAX_SHARED_MEM 10

struct shared_memory
{
  int id;
  int ref_count;
  struct sleeplock lock;
  uint physical_memory;
};

struct shared_memory shared_memory_table[MAX_SHARED_MEM];

int is_init = 0;

void init_shared_memory_table() // is called to make sure the array(table) is init
{
  if (is_init == 0)
  {
    cprintf("init_shared_memory_table called \n");
    for (int i = 0; i < MAX_SHARED_MEM; i++)
    {
      shared_memory_table[i].id = i;
      shared_memory_table[i].ref_count = 0;
      shared_memory_table[i].physical_memory = 0;
      initsleeplock(&shared_memory_table[i].lock, "shared_memory_table");
    }
    is_init = 1;
  }
}

void get_sharedmem(int id, char **pointer)
{
  init_shared_memory_table();
  // acquiresleep(&shared_memory_table[id].lock);
  // cprintf("get_sharedmem called, pid:%d, ref_count:%d\n", myproc()->pid, shared_memory_table[id].ref_count);
  if (shared_memory_table[id].ref_count == 0)
  {
    char *memory = kalloc();
    if (memory == 0)
    {
      panic("get_sharedmem: kalloc failed");
    }
    struct proc *curproc = myproc();
    // allocating from the top of user space
    void *address = (void *)(curproc->top - PGSIZE); // address is virtual, specific to each process
    curproc->top -= PGSIZE;                          // update top for the next allocation
    memset(memory, 0, PGSIZE);
    mappages(curproc->pgdir, address, PGSIZE, V2P(memory), PTE_W | PTE_U);
    *pointer = address;
    shared_memory_table[id].physical_memory = V2P(memory);
  }
  else
  {
    // get physical address from the page table and map it to the virtual address of this new process
    uint physical_address = shared_memory_table[id].physical_memory;
    struct proc *curproc = myproc();
    void *address = (void *)(curproc->top - PGSIZE);
    curproc->top -= PGSIZE;
    mappages(curproc->pgdir, address, PGSIZE, physical_address, PTE_W | PTE_U);
    *pointer = address;
  }
  shared_memory_table[id].ref_count++;
  // releasesleep(&shared_memory_table[id].lock);
}

void dump_sharedmem(int id)
{
  init_shared_memory_table();
  // acquiresleep(&shared_memory_table[id].lock);
  // cprintf("dump_sharedmem called, pid:%d, ref_count:%d\n", myproc()->pid, shared_memory_table[id].ref_count);
  if (id >= MAX_SHARED_MEM)
  {
    cprintf("dump_sharedmem: shared mem id out of index\n");
    return;
  }
  if (shared_memory_table[id].ref_count > 0)
  {
    shared_memory_table[id].ref_count--;
  }
  else
  {
    cprintf("dump_sharedmem: ref_count is 0");
    return;
  }
  if (shared_memory_table[id].ref_count == 0)
  {
    cprintf("ref count hit 0\n");
    char *virtual_address = P2V(shared_memory_table[id].physical_memory);
    memset(virtual_address, 1, PGSIZE);
    kfree(virtual_address);
    shared_memory_table[id].physical_memory = 0;
  }
  // releasesleep(&shared_memory_table[id].lock);
}

void get_sharedmem_lock(int id)
{
  // while (shared_memory_table[id].lock.pid != 0)
  // {
  //   cprintf("ll\n");
  // };
  acquiresleep(&shared_memory_table[id].lock);
}

void let_sharedmem_lock(int id)
{
  // while (shared_memory_table[id].lock.pid == 0)
  // {
  //   cprintf("ee\n");
  // };
  // cprintf("dd\n");
  releasesleep(&shared_memory_table[id].lock);
}

/*
void allocate_shared_memory(int id, char **pointer)
{
  acquire(&shared_memory_table[id].lock);
  cprintf("allocate_shared_memory called \n");
  struct proc *curproc = myproc();
  if (shared_memory_table[id].ref_count == 0)
  { // empty, need to allocate

    uint old_size = curproc->sz;
    curproc->sz += PGSIZE;
    shared_memory_table[id].ref_count++;
    cprintf("here1\n");
    char *memory = (char *)allocuvm(curproc->pgdir, old_size, curproc->sz);
    cprintf("here2\n");

    if (memory == 0)
    {
      panic("allocate_shared_memory: allocuvm failed");
    }
    cprintf("here3\n");
    if (mappages(curproc->pgdir, (char *)curproc->shared_memory, PGSIZE, V2P(memory), PTE_W | PTE_U) < 0)
    {
      panic("allocate_shared_memory: mappages failed");
    }
    memset(memory, 0, PGSIZE);
    cprintf("here4\n");
    for (int i = 0; i < PGSIZE; i++)
    { // check to see if allocation to 0 was successful
      if (memory[i] != 0)
      {
        panic("allocate_shared_memory: memset failed");
      }
    }
    //if (mappages(curproc->pgdir, (void *)PGROUNDUP(curproc->sz), PGSIZE, V2P(memory), PTE_W | PTE_U) < 0)
    //{
    //  panic("allocate_shared_memory: mappages failed");
    //}
    //  mappages: maps the virtual address to the physical address
    //  (void *)PGROUNDUP(curproc->sz): The virtual address to map. It is rounded up to the nearest page boundary using PGROUNDUP(curproc->sz).
    //  PGSIZE: The size of the memory to map
    //  V2P(memory): The physical address to map to
    //  PTE_W | PTE_U: PTE_W -> The page is writable. PTE_U -> The page is accessible in user mode.
    shared_memory_table[id].val = memory;
  }
  else if (shared_memory_table[id].ref_count > 0)
  { // already allocated
    shared_memory_table[id].ref_count++;
    if (mappages(curproc->pgdir, (char *)curproc->shared_memory, PGSIZE, V2P(shared_memory_table[id].val), PTE_W | PTE_U) < 0)
    {
      panic("allocate_shared_memory: mappages failed");
    }
  }

  cprintf("allocate_shared_memory called\n");

  strncpy(shared_memory_table[id].val, "Hello, World!", PGSIZE);
  cprintf("the value for id %d is %s\n", id, shared_memory_table[id].val);
  *pointer = shared_memory_table[id].val;
  release(&shared_memory_table[id].lock);
}
*/
/*
void allocate_shared_memory(int id, char **pointer)
{
  acquire(&shared_memory_table[id].lock);
  cprintf("allocate_shared_memory called \n");
  struct proc *curproc = myproc();
  if (shared_memory_table[id].ref_count == 0)
  { // empty, need to allocate
    char *memory = kalloc();
    if (memory == 0)
    {
      panic("allocate_shared_memory: kalloc failed");
    }
    memset(memory, 0, PGSIZE);

    pte_t *pte = walkpgdir(curproc->pgdir, (void *)curproc->shared_memory, 1);
    cprintf("pte before: %p\n", pte);
    if (pte == 0)
    {
      panic("allocate_shared_memory: walkpgdir failed");
    }
    *pte = V2P(memory) | PTE_W | PTE_U;
    cprintf("pte after: %p\n", pte);

    shared_memory_table[id].ref_count++;
    shared_memory_table[id].val = memory;
    shared_memory_table[id].physical_address = pte;
  }
  else if (shared_memory_table[id].ref_count > 0)
  { // already allocated
    shared_memory_table[id].ref_count++;
    pte_t *pte = walkpgdir(curproc->pgdir, (void *)curproc->shared_memory, 1);
    if (pte == 0)
    {
      panic("allocate_shared_memory: walkpgdir failed");
    }
    *pte = V2P(shared_memory_table[id].val) | PTE_W | PTE_U;
  }
  // Write the value 5 to the first byte of the page
  // strncpy(shared_memory_table[id].val, "Hello, World!", PGSIZE);
  cprintf("the value for id %d is %s\n", id, shared_memory_table[id].val);
  *pointer = shared_memory_table[id].val;
  cprintf("in kernel, pointer is:%p\n", *pointer);
  release(&shared_memory_table[id].lock);
}

void get_sharedmem_old(int id, char **pointer)
{
  cprintf("get_sharedmem called \n");
  init_shared_memory_table();
  if (id >= MAX_SHARED_MEM)
  {
    cprintf("get_sharedmem: shared mem id out of index: %d\n", id);
  }
  // allocate_shared_memory(id, pointer);
}

void dump_sharedmem_2(int id)
{
  cprintf("dump_sharedmem called \n");
  init_shared_memory_table();
  if (id >= MAX_SHARED_MEM)
  {
    cprintf("dump_sharedmem: shared mem id out of index\n");
    return;
  }
  acquire(&shared_memory_table[id].lock);
  if (shared_memory_table[id].ref_count > 0)
  {
    shared_memory_table[id].ref_count--;
  }
  if (shared_memory_table[id].ref_count == 0)
  {
    struct proc *curproc = myproc();
    pte_t *pte = walkpgdir(curproc->pgdir, (void *)curproc->shared_memory, 0);
    if (pte == 0)
    {
      panic("dump_sharedmem: walkpgdir failed");
    }
    char *memory = P2V(PTE_ADDR(*pte));
    kfree(memory);
    *pte = 0;
  }
  release(&shared_memory_table[id].lock);
}
*/
/*
void dump_sharedmem(int id)
{
  cprintf("dump_sharedmem called \n");
  init_shared_memory_table();
  if (id >= MAX_SHARED_MEM)
  {
    cprintf("dump_sharedmem: shared mem id out of index\n");
    return;
  }
  cprintf("here10\n");
  acquire(&shared_memory_table[id].lock);
  cprintf("here11\n");
  if (shared_memory_table[id].ref_count > 0)
  {
    shared_memory_table[id].ref_count--;
  }
  if (shared_memory_table[id].ref_count == 0)
  {
    struct proc *curproc = myproc();
    curproc->sz = deallocuvm(curproc->pgdir, curproc->sz, curproc->sz - PGSIZE);
  }
  cprintf("here12\n");
  release(&shared_memory_table[id].lock);
  cprintf("here13\n");
}*/