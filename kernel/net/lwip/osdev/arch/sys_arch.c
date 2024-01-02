#include <kernel/cpu.h>
#include <kernel/mm/kmem.h>
#include <kernel/mm/page.h>
#include <kernel/kmutex.h>
#include <kernel/kqueue.h>
#include <kernel/ksemaphore.h>
#include <kernel/ktime.h>
#include <kernel/task.h>
#include <kernel/types.h>

#include "lwip/sys.h"
#include "cc.h"

#if !NO_SYS

#include "sys_arch.h"

// Not good in multithreaded environment!

static struct KMemCache *mutex_cache;
static struct KMemCache *queue_cache;
static struct KMemCache *sem_cache;
static struct KMemCache *task_cache;

/* Mutex functions: */

err_t
sys_mutex_new(sys_mutex_t *mutex)
{
  struct KMutex *kmutex;

  if ((kmutex = (struct KMutex *) kmem_alloc(mutex_cache)) == NULL)
    panic("kmem_alloc");
  if (kmutex_init(kmutex, "lwip") != 0)
    panic("kmutex_init");

  *mutex = kmutex;

  return ERR_OK;
}

void
sys_mutex_lock(sys_mutex_t *mutex)
{
  kmutex_lock(*mutex);
}

void
sys_mutex_unlock(sys_mutex_t *mutex)
{
  kmutex_unlock(*mutex);
}

void
sys_mutex_free(sys_mutex_t *mutex)
{
  kmem_free(mutex_cache, *mutex);
}

int
sys_mutex_valid(sys_mutex_t *mutex)
{
  return *mutex != NULL;
}

void
sys_mutex_set_invalid(sys_mutex_t *mutex)
{
  *mutex = NULL;
}

/* Semaphore functions: */

err_t
sys_sem_new(sys_sem_t *sem, u8_t count)
{
  struct KSemaphore *ksem;

  if ((ksem = (struct KSemaphore *) kmem_alloc(sem_cache)) == NULL)
    panic("kmem_alloc");
  if (ksem_create(ksem, count) != 0)
    panic("ksem_create");

  *sem = ksem;

  return ERR_OK;
}

void
sys_sem_signal(sys_sem_t *sem)
{
  ksem_put(*sem);
}

u32_t
sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
  unsigned long start, end;

  start = ktime_get();
  if (ksem_get(*sem, timeout / 10, 1) < 0)
    return SYS_ARCH_TIMEOUT;
  end = ktime_get();
  
  return MIN(timeout, (end - start) * 10);
}

void
sys_sem_free(sys_sem_t *sem)
{
  kmem_free(sem_cache, *sem);
}

int
sys_sem_valid(sys_sem_t *sem)
{
  return *sem != NULL;
}

void
sys_sem_set_invalid(sys_sem_t *sem)
{
  *sem = NULL;
}

// void
// sys_msleep(u32_t ms)
// {
//   task_sleep(ms / 10);
// }

/* Mailbox functions. */

err_t
sys_mbox_new(sys_mbox_t *mbox, int size)
{
  struct KQueue *queue;
  struct Page *page;

  (void) size;

  if ((queue = (struct KQueue *) kmem_alloc(queue_cache)) == NULL)
    panic("kmem_alloc");
  if ((page = page_alloc_one(0)) == NULL)
    panic("page_alloc");
  if (kqueue_init(queue, sizeof(void *), page2kva(page), PAGE_SIZE) < 0)
    panic("kqueue_init");

  *mbox = queue;

  return 0;
}

void
sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
  kqueue_send(*mbox, &msg, 0, 1);
}

err_t
sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
  return kqueue_send(*mbox, &msg, 0, 0);
}

err_t
sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg)
{
  return kqueue_send(*mbox, &msg, 0, 0);
}

u32_t
sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
  unsigned long start, end;

  start = ktime_get();
  if (kqueue_receive(*mbox, msg, timeout / 10, 1) < 0)
    return SYS_ARCH_TIMEOUT;
  end = ktime_get();
  
  return MIN(timeout, (end - start) * 10);
}

u32_t
sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
  if (kqueue_receive(*mbox, msg, 0, 0) < 0)
    return SYS_MBOX_EMPTY;
  return 0;
}

void
sys_mbox_free(sys_mbox_t *mbox)
{
  kmem_free(queue_cache, *mbox);
}

int
sys_mbox_valid(sys_mbox_t *mbox)
{
  return *mbox != NULL;
}

void
sys_mbox_set_invalid(sys_mbox_t *mbox)
{
  *mbox = NULL;
}

sys_thread_t
sys_thread_new(const char *name, void (*thread)(void *), void *arg,
               int stacksize, int prio)
{
  struct Task *task;
  struct Page *stack_page;

  if ((task = (struct Task *) kmem_alloc(task_cache)) == NULL)
    panic("kmem_alloc");
  if ((stack_page = page_alloc_one(0)) == NULL)
    panic("page_alloc");

  // TODO: priority!
  (void) name;
  (void) prio;
  (void) stacksize;

  task_create(task, thread, arg, 0, page2kva(stack_page + 1), NULL);
  task_resume(task);

  return task;
}

void
sys_init(void)
{
  mutex_cache = kmem_cache_create("mutex", sizeof(struct KMutex), 0, NULL, NULL);
  queue_cache = kmem_cache_create("queue", sizeof(struct KQueue), 0, NULL, NULL);
  sem_cache   = kmem_cache_create("sem", sizeof(struct KSemaphore), 0, NULL, NULL);
  task_cache  = kmem_cache_create("task", sizeof(struct Task), 0, NULL, NULL);
}

#endif

int errno;

u32_t
sys_jiffies(void)
{
  return ktime_get();
}

u32_t
sys_now(void)
{
  return ktime_get() * 10;
}

static struct SpinLock lwip_lock = SPIN_INITIALIZER("lwip");

sys_prot_t
sys_arch_protect(void)
{
  spin_lock(&lwip_lock);
  return 0;
}

void
sys_arch_unprotect(sys_prot_t pval)
{
  (void) pval;
  spin_unlock(&lwip_lock);
}