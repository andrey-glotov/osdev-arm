#ifndef __KERNEL_INCLUDE_KERNEL_DRIVERS_CONSOLE_H__
#define __KERNEL_INCLUDE_KERNEL_DRIVERS_CONSOLE_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * @file include/drivers/console.h
 * 
 * General device-independent console code.
 */

#include <stdarg.h>
#include <sys/types.h>
#include <sys/termios.h>

#include <kernel/spinlock.h>
#include <kernel/waitqueue.h>
#include <kernel/drivers/screen.h>

#define TTY_INPUT_MAX     256

struct Tty {
  struct {
    char                buf[TTY_INPUT_MAX];
    size_t              size;
    size_t              read_pos;
    size_t              write_pos;
    struct KSpinLock    lock;
    struct KWaitQueue   queue;
  } in;
  struct {
    struct Screen      *screen;
    int                 stopped;
    struct KSpinLock    lock;
  } out;
  struct termios        termios;
  pid_t                 pgrp;
};

extern struct Tty *tty_current;
extern struct Tty *tty_system;

void    tty_init(void);
void    tty_process_input(struct Tty *, char *);
ssize_t tty_read(dev_t, uintptr_t, size_t);
ssize_t tty_write(dev_t, uintptr_t, size_t);
int     tty_ioctl(dev_t, int, int);
int     tty_select(dev_t, struct timeval *);
void    tty_switch(int);

#endif  // !__KERNEL_INCLUDE_KERNEL_DRIVERS_CONSOLE_H__