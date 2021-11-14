#include <syscall.h>
#include <user.h>

/**
 * Terminate a process.
 * 
 * @param status Exit status of the process.
 */
void
exit(int status)
{
  syscall(SYS_exit, status, 0, 0);
}