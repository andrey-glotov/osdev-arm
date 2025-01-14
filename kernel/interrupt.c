#include <kernel/console.h>
#include <kernel/interrupt.h>
#include <kernel/trap.h>
#include <kernel/core/cpu.h>
#include <kernel/core/irq.h>
#include <kernel/thread.h>
#include <kernel/object_pool.h>

static int  interrupt_handler_call(int);
static void interrupt_thread_entry(void *);
static int  interrupt_thread_notify(int, void *);

// TODO: should be architecture-specific?
#define INTERRUPT_HANDLER_MAX       64

struct InterruptThread {
  interrupt_handler_t handler;
  void               *handler_arg;
  int                 irq;
  struct KSemaphore   semaphore;
};

static struct {
  interrupt_handler_t handler;
  void *handler_arg;
} interrupt_handlers[INTERRUPT_HANDLER_MAX];

void
interrupt_attach(int irq, interrupt_handler_t handler, void *handler_arg)
{
  if ((irq < 0) || (irq >= INTERRUPT_HANDLER_MAX))
    panic("invalid interrupt id %d", irq);

  if (interrupt_handlers[irq].handler != NULL)
    panic("interrupt handler %d already attached", irq);

  interrupt_handlers[irq].handler     = handler;
  interrupt_handlers[irq].handler_arg = handler_arg;

  arch_interrupt_enable(irq, k_cpu_id());
  arch_interrupt_unmask(irq);
}

void
interrupt_attach_thread(int irq, interrupt_handler_t handler, void *handler_arg)
{
  struct InterruptThread *isr;
  struct KThread *thread;

  if ((isr = k_malloc(sizeof(struct InterruptThread))) == NULL)
    panic("cannot allocate IRQ thread strucure");

  if ((thread = k_thread_create(NULL, interrupt_thread_entry, isr, 0)) == NULL)
    panic("cannot create IRQ thread");

  k_semaphore_init(&isr->semaphore, 0);
  isr->irq         = irq;
  isr->handler     = handler;
  isr->handler_arg = handler_arg;

  interrupt_attach(irq, interrupt_thread_notify, isr);

  k_thread_resume(thread);
}

void
interrupt_dispatch(void)
{
  int irq = arch_interrupt_id();
  int should_unmask;

  k_irq_handler_begin();

  arch_interrupt_mask(irq);
  arch_interrupt_eoi(irq);

  should_unmask = interrupt_handler_call(irq);
  if (should_unmask)
    arch_interrupt_unmask(irq);

  k_irq_handler_end();
}

static int
interrupt_handler_call(int irq)
{
  if (interrupt_handlers[irq].handler)
    return interrupt_handlers[irq].handler(irq,
                                           interrupt_handlers[irq].handler_arg);
 
  // TODO: warn
  cprintf("Unexpected IRQ %d from CPU %d\n", irq, k_cpu_id());

  return 1;
}

static void
interrupt_thread_entry(void *arg)
{
  struct InterruptThread *isr = (struct InterruptThread *) arg;

  for (;;) {
    int should_unmask;

    if (k_semaphore_get(&isr->semaphore) < 0)
      panic("k_semaphore_get");

    should_unmask = isr->handler(isr->irq, isr->handler_arg);
    if (should_unmask) {
      arch_interrupt_unmask(isr->irq);
    }
  }
}

static int
interrupt_thread_notify(int irq, void *arg)
{
  struct InterruptThread *isr = (struct InterruptThread *) arg;

  (void) irq;
  k_semaphore_put(&isr->semaphore);

  // Do not re-enable the interrupt now, the handler thread will do it
  return 0;
}

// TODO: detach
