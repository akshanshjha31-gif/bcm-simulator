/**
 * @file    syscalls.c
 * @brief   Minimal newlib system-call stubs for a bare-metal target.
 *
 * Only _sbrk is functionally required (heap growth for malloc/new). The rest
 * are inert stubs so the linker is satisfied. _write is later re-pointed at
 * the diagnostic UART so printf-style logging can reach the desktop tool.
 */
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

/* Provided by the linker script. */
extern char end;          /* first address past .bss = start of heap */
extern char _estack;      /* top of RAM / stack                      */

register char *stack_ptr asm("sp");

caddr_t _sbrk(int incr)
{
    static char *heap = 0;
    char *prev;

    if (heap == 0) {
        heap = &end;
    }
    prev = heap;

    /* Refuse to let the heap collide with the current stack pointer. */
    if (heap + incr > stack_ptr) {
        errno = ENOMEM;
        return (caddr_t)-1;
    }
    heap += incr;
    return (caddr_t)prev;
}

int   _write(int file, char *ptr, int len) { (void)file; (void)ptr; return len; }
int   _read(int file, char *ptr, int len)  { (void)file; (void)ptr; (void)len; return 0; }
int   _close(int file)                     { (void)file; return -1; }
int   _fstat(int file, struct stat *st)    { (void)file; st->st_mode = S_IFCHR; return 0; }
int   _isatty(int file)                    { (void)file; return 1; }
int   _lseek(int file, int off, int dir)   { (void)file; (void)off; (void)dir; return 0; }
int   _getpid(void)                        { return 1; }
int   _kill(int pid, int sig)              { (void)pid; (void)sig; errno = EINVAL; return -1; }
void  _exit(int status)                    { (void)status; while (1) { } }
