/* newlib needs _sbrk if anything calls malloc. This project does not use
 * the C heap (FreeRTOS has its own heap_4), so _sbrk just fails. If
 * something unexpected calls malloc we see it as a NULL return instead
 * of silently eating the RAM that belongs to the stack. */
#include <errno.h>
#include <stddef.h>

void *_sbrk(ptrdiff_t incr)
{
    (void)incr;
    errno = ENOMEM;
    return (void *)-1;
}

/* Stubs so newlib links without warnings. Nothing in this project uses
 * file I/O (printing goes through bsp_log -> vsnprintf -> UART). */
int _write(int fd, const char *buf, int len) { (void)fd; (void)buf; (void)len; errno = ENOSYS; return -1; }
int _read(int fd, char *buf, int len)        { (void)fd; (void)buf; (void)len; errno = ENOSYS; return -1; }
int _close(int fd)                           { (void)fd; errno = ENOSYS; return -1; }
int _lseek(int fd, int off, int whence)      { (void)fd; (void)off; (void)whence; errno = ENOSYS; return -1; }
