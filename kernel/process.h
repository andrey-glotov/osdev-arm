#ifndef __KERNEL_PROCESS_H__
#define __KERNEL_PROCESS_H__

#include "list.h"
#include <sys/types.h>

#include "mmu.h"
#include "trap.h"

enum {
  PROCESS_EMBRIO   = 1,
  PROCESS_RUNNABLE = 2,
  PROCESS_RUNNING  = 3,
  PROCESS_SLEEPING = 4,
  PROCESS_ZOMBIE   = 5,
};

/**
 * Saved registers for kernel context swithces.
 */
struct Context {
  uint32_t r4;
  uint32_t r5;
  uint32_t r6;
  uint32_t r7;
  uint32_t r8;
  uint32_t r9;
  uint32_t r10;
  uint32_t r11;
  uint32_t lr;
};

/**
 * Process state.
 */
struct Process {
  struct ListLink    link;      ///< Link into the containing list
  int                state;     ///< Process state

  pid_t              pid;       ///< Process identifier
  struct ListLink    pid_link;  ///< Link into the PID hash table

  struct Process    *parent;    ///< Link to the parent process
  struct ListLink    children;  ///< List pf process children
  struct ListLink    sibling;   ///< Link into the siblings list

  tte_t             *trtab;     ///< Translation table
  size_t             size;      ///< Size of process memory (in bytes)

  uint8_t           *kstack;    ///< Bottom of process kernel stack
  struct Trapframe  *tf;        ///< Trap frame for current exception
  struct Context    *context;   ///< Saved context
};

#define NCPU  4

/**
 * Per-CPU state.
 */
struct Cpu {
  struct Context *scheduler;    ///< Saved scheduler context
  struct Process *process;      ///< The currently running process   
  int             irq_lock;     ///< Depth of IRQ lock nesting
  int             irq_flags;    ///< Were interupts enabled before IRQ lock?
};

extern struct Cpu cpus[];

void            context_switch(struct Context **old, struct Context *new);

void            process_init(void);
int             process_create(const void *);
void            process_yield(void);
void            process_destroy(int);
void            process_free(struct Process *);
pid_t           process_copy(void);
void            scheduler(void);

int             cpuid(void);
struct Cpu     *mycpu(void);
struct Process *myprocess(void);

extern int nprocesses;

#define PROCESS_PASTE3(x, y, z)   x ## y ## z

#define PROCESS_CREATE(name)                                                  \
  do {                                                                        \
    extern uint8_t PROCESS_PASTE3(_binary_obj_user_, name, _start)[];         \
                                                                              \
    if (process_create(PROCESS_PASTE3(_binary_obj_user_, name, _start)) < 0)  \
      panic("cannot create process '%s'", #name);                             \
  } while (0)

#endif  // __KERNEL_PROCESS_H__
