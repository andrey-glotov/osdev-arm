#include <argentum/kthread.h>
#include <argentum/spinlock.h>
#include <argentum/wchan.h>

/**
 * Initialize the wait channel.
 * 
 * @param chan A pointer to the channel to be initialized.
 */
void
wchan_init(struct WaitChannel *chan)
{
  list_init(&chan->head);
}

/**
 * Wait for the resource associated with the given wait channel to become
 * available and release an optional spinlock.
 * 
 * @param chan A pointer to the wait channel to sleep on.
 * @param lock A pointer to the spinlock to be released.
 */
void
wchan_sleep(struct WaitChannel *chan, struct SpinLock *lock)
{
  kthread_sleep(&chan->head, KTHREAD_SLEEPING_WCHAN, lock);
}

/**
 * Wakeup the highest-priority thread sleeling on the wait channel.
 * 
 * @param chan A pointer to the wait channel
 */
void
wchan_wakeup_one(struct WaitChannel *chan)
{
  kthread_wakeup_one(&chan->head);
}

/**
 * Wakeup all threads sleeling on the wait channel.
 * 
 * @param chan A pointer to the wait channel
 */
void
wchan_wakeup_all(struct WaitChannel *chan)
{
  kthread_wakeup_all(&chan->head);
}
