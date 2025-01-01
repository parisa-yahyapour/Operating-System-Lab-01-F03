// Mutual exclusion lock.
struct spinlock
{
  uint locked; // Is the lock held?

  // For debugging:
  char *name;      // Name of lock.
  struct cpu *cpu; // The cpu holding the lock.
  uint pcs[10];    // The call stack (an array of program counters)
                   // that locked the lock.
};

struct reentrantlock
{
  struct spinlock lock; 
  struct proc *owner;  
  int recursion;        
};

void Initreentrantlock(struct reentrantlock *rlock, char *name);
void acquirereentrantlock(struct reentrantlock *rlock);
void releasereentrantlock(struct reentrantlock *rlock);

