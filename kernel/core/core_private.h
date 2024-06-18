#ifndef __CORE_PRIVATE_H
#define __CORE_PRIVATE_H

#include <kernel/spinlock.h>

struct Context;
struct KThread;
struct ListLink;

void _k_sched_resume(struct KThread *, int);
void _k_sched_may_yield(struct KThread *);
void _k_sched_yield(void);
void _k_sched_enqueue(struct KThread *);
void _k_sched_wakeup_all(struct ListLink *, int);
void _k_sched_wakeup_one(struct ListLink *, int);
int  _k_sched_sleep(struct ListLink *, int, unsigned long, struct KSpinLock *);

extern struct KSpinLock _k_sched_spinlock;

static inline void
_k_sched_spin_lock(void)
{
  k_spinlock_acquire(&_k_sched_spinlock);
}

static inline void
_k_sched_spin_unlock(void)
{
  k_spinlock_release(&_k_sched_spinlock);
}

/**
 * The kernel maintains a special structure for each processor, which
 * records the per-CPU information.
 */
struct KCpu {
  struct Context *sched_context;  ///< Saved scheduler context
  struct KThread *thread;         ///< The currently running kernel task
  int             lock_count;     ///< Sheculer lock nesting level
  int             irq_save_count; ///< Nesting level of k_irq_save() calls
  int             irq_flags;      ///< IRQ state before the first k_irq_save()
};

struct KCpu    *_k_cpu(void);

#endif  // !__CORE_PRIVATE_H
