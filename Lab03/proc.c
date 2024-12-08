#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

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
  p->tick_count = 0;
  p->consecutive_run = 0;

  if (p->pid == 1 || p->pid == 2) // || p->parent->pid == 2
  {
    p->arrival_time = ticks;
    p->priority_level = 1;
  }
  else
  {
    p->arrival_time = ticks;
    p->priority_level = 3;
  }
  // p->priority_level = 1; // i change it damet
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
  p->confidence = 50;
  p->time_burst = 2;
  p->is_checked = 0;

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;
  p->ticks_queued = ticks; // Update when process enters the ready queue

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
  np->confidence = 50;
  np->time_burst = 2;
  np->is_checked = 0;

  for (i = 0; i < NOFILE; i++)
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;
  np->ticks_queued = ticks; // Update when process enters the ready queue

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
unsigned int seed = 1; // Global seed value, should be initialized

// Function to generate the next pseudo-random number
int generate_random_number(unsigned int min, unsigned int max)
{
  if (min > max)
  {
    return 0; // Error: invalid range
  }

  // Constants for the LCG (these values are common choices)
  const unsigned int a = 1103515245;
  const unsigned int c = 12345;
  const unsigned int m = 0x7FFFFFFF; // 2^31 - 1

  // Update the seed using the LCG formula
  seed = (a * seed + c) & m;

  // Scale the result to the desired range
  return (seed % 100);
}

struct proc *FCFS(void)
{
  struct proc *p;
  struct proc *chosen_proc;
  struct cpu *c = mycpu(); // Get the current CPU

  // c->proc = 0; // No process is running initially

  chosen_proc = 0;

  // Select the process with the smallest ticks_queued
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state != RUNNABLE || mycpu()->ps_priority != 3 || p->priority_level != 3)
      continue;

    if (!chosen_proc || p->ticks_queued < chosen_proc->ticks_queued)
    {
      chosen_proc = p;
    }
  }

  if (chosen_proc)
  {
    // Switch to the chosen process
    c->proc = chosen_proc;
    switchuvm(c->proc);
    c->proc->state = RUNNING;
    c->proc->queue_waiting_time = 0;

    swtch(&c->scheduler, c->proc->context); // Context switch
    switchkvm();

    // Process is done running
    c->proc = 0;
  }

  return 0;
}

struct proc *Round_Robin(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {

    // cprintf("in RR:      %d\n", mycpu()->ps_priority);

    if (p->state != RUNNABLE || p->priority_level != 1 || mycpu()->ps_priority != 1)
      continue;

    // Switch to chosen process.  It is the process's job
    // to release ptable.lock and then reacquire it
    // before jumping back to us.
    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;
    c->proc->queue_waiting_time = 0;

    swtch(&(c->scheduler), p->context);
    switchkvm();

    // Process is done running for now.
    // It should have changed its p->state before coming back.
    c->proc = 0;
  }
  return 0;
}

struct proc *SJF(void)
{
  struct proc *p, *shortest = 0;
  // struct cpu *c = mycpu();
  // c->proc = 0;
  shortest = 0;
  // cprintf("a\n");
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if ((p->state != RUNNABLE || mycpu()->ps_priority != 2) || p->priority_level != 2)
      continue;
    // cprintf("shortest %d\n", p->time_burst);
    if ((!shortest || p->time_burst < shortest->time_burst) && p->is_checked == 0 && p->pid != 0)
    {
      // cprintf("b\n");
      shortest = p;
    }
    // Switch to chosen process.  It is the process's job
    // to release ptable.lock and then reacquire it
    // before jumping back to us.
  }
  // cprintf("c\n");
  if (shortest)
  {
    // cprintf("d\n");
    int random = generate_random_number(0, 100);
    // int random = 10;
    if (shortest->confidence > random)
    {
      p = shortest;
      // c->proc = p;
      struct proc *temp;
      for (temp = ptable.proc; temp < &ptable.proc[NPROC]; temp++)
      {
        temp->is_checked = 0;
      }
      return p;
      // switchuvm(p);
      // p->state = RUNNING;

      // swtch(&(c->scheduler), p->context);
      // switchkvm();

      // // Process is done running for now.
      // // It should have changed its p->state before coming back.
      // c->proc = 0;
      // // cprintf("e\n");
    }
    else
    {
      // cprintf("f\n");
      shortest->is_checked = 1;
    }
  }
  return 0;
}

extern int change_queue(int pid, int new_queue);

void update_age(void)
{
  struct proc *p;
  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == RUNNABLE)
    {
      p->queue_waiting_time++;
      if (p->queue_waiting_time == 800)
      {
        p->queue_waiting_time = 0;
        int priority = p->priority_level;
        switch (priority)
        {
        case 2:
        {
          int old_queue = change_queue(p->pid, 1);
          cprintf("Pid: %d, Source: %d, Destination : %d\n", p->pid, old_queue, 1);
        }
        break;
        case 3:
        {
          int old_queue_2 = change_queue(p->pid, 2);
          cprintf("Pid: %d, Source: %d, Destination: %d\n", p->pid, old_queue_2, 2);
        }
        break;
        default:
          break;
        }
      }
    }
  }
  release(&ptable.lock);
}

void scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  for (;;)
  {
    // Enable interrupts on this CPU
    sti();

    // Lock process table to search for a runnable process
    acquire(&ptable.lock);
    p = Round_Robin();
    if (p == 0)
    {
      mycpu()->ps_priority = 2;

      p = SJF();
      // cprintf("ooo\n");
    }
    if (p == 0)
    {
      mycpu()->ps_priority = 3;
      p = FCFS();
      // cprintf("pppp\n");
    }

    // Call FCFS to find the next process to run
    // struct proc* process_found = FCFS();
    // if (process_found) {
    //     cprintf("Process selected: %d\n", process_found);
    // }
    if (p == 0)
    {

      mycpu()->ps_priority = 1;
      mycpu()->fcfs = 100;
      mycpu()->sjf = 200;
      mycpu()->rr = 300;
      release(&ptable.lock);
      // cprintf("rrrr\n");

      continue;
    }

    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;
    c->proc->queue_waiting_time = 0;

    swtch(&(c->scheduler), p->context);
    switchkvm();

    // // Process is done running for now.
    // // It should have changed its p->state before coming back.
    c->proc = 0;

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
  myproc()->ticks_queued = ticks; // Update when process enters the ready queue
  sched();
  release(&ptable.lock);
}

void wrr_yeild(void)
{
  struct proc *p;
  int rr_count = 0, fcfs_count = 0, sjf_count = 0;
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == RUNNABLE)
    {
      switch (p->priority_level)
      {
      case 1:
        rr_count += 1;
        break;
      case 2:
        sjf_count += 1;
        break;
      case 3:
        fcfs_count += 1;
        break;
      default:
        break;
      }
    }
  }
  int priority = mycpu()->proc->priority_level;
  // cprintf("%d\n",priority);

  if (priority == 1)
  {
    if (mycpu()->rr == 0 || rr_count == 0)
    {
      mycpu()->ps_priority = 2;
      // cprintf("%d\n", mycpu()->ps_priority);
      // cprintf("uuuuuuu yeild\n");

      yield();
    }
    else
    {

      return;
    }
  }
  else if (priority == 2)
  {
    if (mycpu()->sjf == 0 || sjf_count == 0)
    {
      mycpu()->ps_priority = 3;
      // cprintf("uuuuuuu yeild2\n");

      yield();

      // berim saf badi damet
    }
    else
    {
      return;
    }
  }
  else if (priority == 3)
  {
    if (mycpu()->fcfs == 0 || fcfs_count == 0)
    {
      mycpu()->ps_priority = 1;
      mycpu()->fcfs = 100;
      mycpu()->sjf = 200;
      mycpu()->rr = 300;
      // cprintf("uuuuuuu yeild3\n");

      yield();

      // hame chi bargardeh
      //  berim saf badi
    }
    else
    {
      return;
    }
  }
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
    {
      p->state = RUNNABLE;
      p->ticks_queued = ticks; // Update when process enters the ready queue
    }
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
      {
        p->state = RUNNABLE;
        p->ticks_queued = ticks; // Update when process enters the ready queue
      }
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

int change_queue(int pid, int new_queue)
{
  struct proc *p;
  int old_queue = -1;
  acquire(&ptable.lock);

  // Find the process with the given pid and change its queue
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      // cprintf("pre: %d",p->priority_level);

      old_queue = p->priority_level;
      p->priority_level = new_queue;
      p->arrival_time = ticks;
      // cprintf("post: %d",p->priority_level);

      // p->sched_info.arrival_queue_time = ticks; // Update the arrival time for the new queue
      break;
    }
  }

  // Release the process table lock
  release(&ptable.lock);

  return old_queue;
}

void set_process_parameter(int pid, int confidence, int time_burst)
{
  struct proc *p = 0;
  // cprintf("%d %d %d\n", time_burst, confidence, pid);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      break;
    }
  }
  if (p != 0)
  {
    p->time_burst = time_burst;
    p->confidence = confidence;
  }
}

int count_digits(int number) {
    if (number == 0)
        return 1;  // Special case: 0 has 1 digit

    int count = 0;
    if (number < 0) {
        count++;       // Count the negative sign
        number = -number; // Work with the absolute value
    }

    while (number > 0) {
        count++;
        number /= 10; // Remove the last digit
    }
    return count;
}

void printspaces(int count) {
    for (int i = 0; i < count; i++) {
        cprintf(" ");
    }
}

void print_process_info(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      [EMBRYO] "embryo",
      [SLEEPING] "sleeping",
      [RUNNABLE] "runnable",
      [RUNNING] "running",
      [ZOMBIE] "zombie"};

 
  static int columns[] = {16, 6, 14, 9, 12, 13, 15, 16, 16};
  cprintf("name           pid     state      queue   wait_time   confidence   burst_time   consecutive_run   arrival\n");
  cprintf("--------------------------------------------------------------------------------------------------------------\n");

  struct proc *p;
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;

    const char *state;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";

    cprintf("%s", p->name);
    printspaces(columns[0] - strlen(p->name));

    cprintf("%d", p->pid);
    printspaces(columns[1] - count_digits(p->pid));

    cprintf("%s", state);
    printspaces(columns[2] - strlen(state));

    cprintf("%d", p->priority_level);
    printspaces(columns[3] - count_digits(p->priority_level));

    cprintf("%d", p->queue_waiting_time);
    printspaces(columns[4] - count_digits(p->queue_waiting_time));

    cprintf("%d", p->confidence);
    printspaces(columns[5] - count_digits(p->confidence));

    cprintf("%d", p->time_burst);
    printspaces(columns[6] - count_digits(p->time_burst));

    cprintf("%d", p->consecutive_run);
    printspaces(columns[7] - count_digits(p->consecutive_run));

    cprintf("%d", p->arrival_time);
    printspaces(columns[8] - count_digits(p->arrival_time));

    cprintf("\n");
  }
}
