#include <kernel/process.h>

int
arch_process_copy(struct Process *parent, struct Process *child)
{
  *child->thread->tf = *parent->thread->tf;
  child->thread->tf->r0 = 0;

  return 0;
}
