#include <kernel/assert.h>
#include <errno.h>
#include <string.h>

#include <kernel/cprintf.h>
#include <kernel/cpu.h>
#include <kernel/irq.h>
#include <kernel/task.h>
#include <kernel/spinlock.h>
#include <kernel/process.h>
#include <kernel/vmspace.h>
#include <kernel/mm/vm.h>
#include <kernel/mm/mmu.h>
#include <kernel/mm/kmem.h>
#include <kernel/mm/page.h>

static void task_run(void);
void context_switch(struct Context **, struct Context *);

static struct ListLink sched_queue[TASK_MAX_PRIORITIES];
struct SpinLock __sched_lock = SPIN_INITIALIZER("sched");

static void task_sleep_callback(void *arg);

/**
 * Initialize the scheduler data structures.
 * 
 * This function must be called prior to creating any kernel tasks.
 */
void
sched_init(void)
{
  int i;

  thread_cache = kmem_cache_create("thread_cache", sizeof(struct Task), 0, NULL, NULL);
  if (thread_cache == NULL)
    panic("cannot allocate thread cache");

  for (i = 0; i < TASK_MAX_PRIORITIES; i++)
    list_init(&sched_queue[i]);
}

// Add the specified task to the run queue with the corresponding priority
static void
sched_enqueue(struct Task *th)
{
  assert(spin_holding(&__sched_lock));

  th->state = TASK_STATE_READY;
  list_add_back(&sched_queue[th->priority], &th->link);
}

// Retrieve the highest-priority task from the run queue
static struct Task *
sched_dequeue(void)
{
  struct ListLink *link;
  int i;
  
  assert(spin_holding(&__sched_lock));

  for (i = 0; i < TASK_MAX_PRIORITIES; i++) {
    if (!list_empty(&sched_queue[i])) {
      link = sched_queue[i].next;
      list_remove(link);

      return LIST_CONTAINER(link, struct Task, link);
    }
  }

  return NULL;
}

/**
 * Start the scheduler main loop. This function never returns.
 */
void
sched_start(void)
{
  struct Cpu *my_cpu;

  sched_lock();

  my_cpu = cpu_current();

  for (;;) {
    struct Task *next = sched_dequeue();

    if (next != NULL) {
      assert(next->state == TASK_STATE_READY);

      if (next->process != NULL)
        vm_load(next->process->vm->pgdir);

      next->state = TASK_STATE_RUNNING;

      next->cpu = my_cpu;
      my_cpu->task = next;

      context_switch(&my_cpu->scheduler, next->context);

      my_cpu->task = NULL;
      next->cpu = NULL;

      if (next->process != NULL)
        vm_load_kernel();

      // Perform cleanup for the exited task
      if (next->state == TASK_STATE_DESTROYED) {
        struct Page *kstack_page;

        next->state = TASK_STATE_NONE;
  
        sched_unlock();

        // Free the kernel stack
        kstack_page = kva2page(next->kstack);
        kstack_page->ref_count--;
        page_free_one(kstack_page);

        kmem_free(thread_cache, next);

        sched_lock();
      }
    } else {
      sched_unlock();

      cpu_irq_enable();
      asm volatile("wfi");

      sched_lock();
    }
  }
}

// Switch back from the current task context back to the scheduler loop
static void
sched_yield(void)
{
  int irq_flags;

  assert(spin_holding(&__sched_lock));

  irq_flags = cpu_current()->irq_flags;
  context_switch(&task_current()->context, cpu_current()->scheduler);
  cpu_current()->irq_flags = irq_flags;
}

/**
 * Notify the kernel that an ISR processing has started.
 */
void
sched_isr_enter(void)
{
  cpu_current()->isr_nesting++;
}

/**
 * Notify the kernel that an ISR processing is finished.
 */
void
sched_isr_exit(void)
{
  struct Cpu *my_cpu;

  sched_lock();

  my_cpu = cpu_current();

  if (my_cpu->isr_nesting <= 0)
    panic("isr_nesting <= 0");

  if (--my_cpu->isr_nesting == 0) {
    struct Task *my_task = my_cpu->task;

    // Before resuming the current task, check whether it must give up the CPU
    // or exit.
    if ((my_task != NULL) && (my_task->flags & TASK_FLAGS_RESCHEDULE)) {
      my_task->flags &= ~TASK_FLAGS_RESCHEDULE;

      sched_enqueue(my_task);
      sched_yield();
    }
  }

  sched_unlock();
}


// Compare task priorities. Note that a smaller priority value corresponds
// to a higher priority! Returns a number less than, equal to, or greater than
// zero if t1's priority is correspondingly less than, equal to, or greater than
// t2's priority.
static int
task_priority_cmp(struct Task *t1, struct Task *t2)
{
  return t2->priority - t1->priority; 
}

// Check whether a reschedule is required (taking into account the priority
// of a task most recently added to the run queue)
static void
sched_may_yield(struct Task *candidate)
{
  struct Cpu *my_cpu;
  struct Task *my_task;

  assert(spin_holding(&__sched_lock));

  my_cpu  = cpu_current();
  my_task = my_cpu->task;

  if ((my_task != NULL) && (task_priority_cmp(candidate, my_task) > 0)) {
    if (my_cpu->isr_nesting > 0) {
      // Cannot yield right now, delay until the last call to sched_isr_exit()
      // or task_unlock().
      my_task->flags |= TASK_FLAGS_RESCHEDULE;
    } else {
      sched_enqueue(my_task);
      sched_yield();
    }
  }
}

void
sched_wakeup_all(struct ListLink *task_list, int result)
{
  if (!spin_holding(&__sched_lock))
    panic("sched not locked");
  
  while (!list_empty(task_list)) {
    struct ListLink *link = task_list->next;
    struct Task *task = LIST_CONTAINER(link, struct Task, link);

    list_remove(link);

    task->sleep_result = result;

    sched_enqueue(task);
    sched_may_yield(task);
  }
}

/**
 * Wake up the task with the highest priority.
 *
 * @param queue Pointer to the head of the wait queue.
 */
void
sched_wakeup_one(struct ListLink *queue, int result)
{
  struct ListLink *l;
  struct Task *highest;

  if (!spin_holding(&__sched_lock))
    panic("sched not locked");

  highest = NULL;

  LIST_FOREACH(queue, l) {
    struct Task *t = LIST_CONTAINER(l, struct Task, link);
    
    if ((highest == NULL) || (task_priority_cmp(t, highest) > 0))
      highest = t;
  }

  if (highest != NULL) {
    // cprintf("wakeup %p from %p\n", highest, queue);

    list_remove(&highest->link);
    highest->sleep_result = result;

    sched_enqueue(highest);
    sched_may_yield(highest);
  }
}

/**
 * Put the current task into sleep.
 *
 * @param queue An optional queue to insert the task into.
 * @param state The state indicating a kind of sleep.
 * @param lock  An optional spinlock to release while going to sleep.
 */
int
sched_sleep(struct ListLink *queue, unsigned long timeout,
            struct SpinLock *lock)
{
  struct Task *my_task = task_current();

  // someone may call this function while holding __sched_lock?
  if (lock != NULL) {
    sched_lock();
    spin_unlock(lock);
  }

  assert(spin_holding(&__sched_lock));

  if (timeout != 0) {
    my_task->sleep_timer.remain = timeout;
    ktimer_start(&my_task->sleep_timer);
  }

  my_task->state = TASK_STATE_SLEEPING;

  if (queue != NULL)
    list_add_back(queue, &my_task->link);

  sched_yield();

  if (timeout != 0)
    ktimer_stop(&my_task->sleep_timer);

  // someone may call this function while holding __sched_lock?
  if (lock != NULL) {
    sched_unlock();
    spin_lock(lock);
  }

  return my_task->sleep_result;
}

void
task_cleanup(struct Task *task)
{
  ktimer_destroy(&task->sleep_timer);
  task->state = TASK_STATE_DESTROYED;
}

static void
task_sleep_callback(void *arg)
{
  struct Task *task = (struct Task *) arg;

  sched_lock();

  if (task->state == TASK_STATE_SLEEPING) {
    task->sleep_result = -ETIMEDOUT;

    list_remove(&task->link);
    sched_enqueue(task);

    sched_may_yield(task);
  }

  sched_unlock();
}

/**
 * Resume execution of a previously suspended task (or begin execution of a
 * newly created one).
 * 
 * @param task The kernel task to resume execution
 */
int
task_resume(struct Task *task)
{
  sched_lock();

  if (task->state != TASK_STATE_SUSPENDED) {
    sched_unlock();
    return -EINVAL;
  }

  sched_enqueue(task);
  sched_may_yield(task);

  sched_unlock();

  return 0;
}

/**
 * Relinguish the CPU allowing another task to be run.
 */
void
task_yield(void)
{
  struct Task *current = task_current();
  
  if (current == NULL)
    panic("no current task");

  sched_lock();

  sched_enqueue(current);
  sched_yield();

  sched_unlock();
}

// Execution of each task begins here.
static void
task_run(void)
{
  struct Task *my_task = task_current();

  // Still holding the scheduler lock (acquired in sched_start)
  sched_unlock();

  // Make sure IRQs are enabled
  cpu_irq_enable();

  // Jump to the task entry point
  my_task->entry(my_task->arg);

  // Destroy the task on exit
  task_exit();
}

/**
 * Initialize the kernel task. After successful initialization, the task
 * is put into suspended state and must be explicitly made runnable by a call
 * to task_resume().
 * 
 * @param process  Pointer to a process the task belongs to.
 * @param task   Pointer to the kernel task to be initialized.
 * @param entry    task entry point function.
 * @param priority task priority value.
 * @param stack    Top of the task stack.
 * 
 * @return 0 on success.
 */
struct Task *
task_create(struct Process *process, void (*entry)(void *), void *arg,
            int priority)
{
  struct Page *stack_page;
  struct Task *task;
  uint8_t *stack, *sp;

  if ((task = (struct Task *) kmem_alloc(thread_cache)) == NULL)
    return NULL;

  if ((stack_page = page_alloc_one(0)) == NULL) {
    kmem_free(thread_cache, task);
    return NULL;
  }

  stack = (uint8_t *) page2kva(stack_page);
  stack_page->ref_count++;

  task->flags         = 0;
  task->priority      = priority;
  task->state         = TASK_STATE_SUSPENDED;
  task->entry         = entry;
  task->arg           = arg;
  task->err           = 0;
  task->process       = process;
  task->kstack        = stack;
  task->tf            = NULL;

  ktimer_create(&task->sleep_timer, task_sleep_callback, task, 0, 0, 0);

  sp = (uint8_t *) stack + PAGE_SIZE;

  if (process != NULL) {
    sp -= sizeof(struct TrapFrame);
    task->tf = (struct TrapFrame *) sp;
    memset(task->tf, 0, sizeof(struct TrapFrame));
  }

  sp -= sizeof *task->context;
  task->context = (struct Context *) sp;
  memset(task->context, 0, sizeof *task->context);
  task->context->lr = (uint32_t) task_run;

  return task;
}

/**
 * Destroy the specified task
 */
void
task_exit(void)
{
  struct Task *task = task_current();

  if (task == NULL)
    panic("currnet task is NULL");

  sched_lock();

  task_cleanup(task);

  sched_yield();
  panic("should not return");
}

/**
 * Get the currently executing task.
 * 
 * @return A pointer to the currently executing task or NULL
 */
struct Task *
task_current(void)
{
  struct Task *task;

  cpu_irq_save();
  task = cpu_current()->task;
  cpu_irq_restore();

  return task;
}
