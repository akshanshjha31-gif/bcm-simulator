/**
 * @file    cpp_runtime.cpp
 * @brief   Minimal freestanding C++ runtime support.
 *
 * The firmware is built with -fno-exceptions -fno-rtti and the nano runtime,
 * so only a handful of runtime hooks are required. Dynamic allocation is
 * routed through newlib malloc/free (heap bounded by the linker script); the
 * BCM itself favours static storage and avoids new/delete on hot paths.
 */
#include <cstddef>
#include <cstdlib>

extern "C" void __cxa_pure_virtual(void)
{
    /* A pure virtual was called - unrecoverable design error. Trap. */
    while (1) { }
}

/* __dso_handle and __cxa_atexit are supplied by the (nano) C++ runtime. */

void *operator new(std::size_t size)        { return std::malloc(size); }
void *operator new[](std::size_t size)      { return std::malloc(size); }
void  operator delete(void *ptr) noexcept   { std::free(ptr); }
void  operator delete[](void *ptr) noexcept { std::free(ptr); }
void  operator delete(void *ptr, std::size_t) noexcept   { std::free(ptr); }
void  operator delete[](void *ptr, std::size_t) noexcept { std::free(ptr); }
