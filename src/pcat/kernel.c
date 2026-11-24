/* Integer-only protected small kernel. No DOS/BIOS calls or hosted CRT. */
#include "mailbox.h"
#include "request.h"
#include "request.c"
#include "dispatch.c"
#include "heapall.c"

static pm_word request_heap_extent(pm_dword address, pm_dword count)
{
    return heap_buffer_extent((const struct heap_state *)HEAP_KERNEL_STATE,
        address, count) == HEAP_OK ? 0 : PM_REQUEST_BAD_EXTENT;
}

u16 kernel_main(void) {
    volatile struct mailbox *m = MAILBOX;
#ifdef _MSC_VER
    volatile u16 _far *window = (volatile u16 _far *)0x00180000UL;
#else
    volatile u16 __far *window =
        (volatile u16 __far *)__builtin_ia16_make_far(0x18, 0);
#endif
    u16 sum = m->seed;
    u16 i, status;
    if (m->magic != 0x504d || !(machine_msw() & 1))
        return 1;
    m->start_msw = machine_msw();
    m->checks = 1;
    for (i = 0; i != 32; ++i) {
        sum = (u16)((sum << 1) | (sum >> 15));
        sum ^= (u16)(m->seed + (u16)(i * 17));
    }
    m->result = sum;
    m->memory_sum = 0;
    for (i = 0; i != 16; ++i)
        window[i] = (u16)(sum ^ i);
    for (i = 0; i != 16; ++i) {
        if (window[i] != (u16)(sum ^ i))
            return 2;
        m->memory_sum = (u16)(m->memory_sum + window[i]);
    }
    m->checks |= 2;
    if (machine_far_call() != 0x20)
        return 3;
    m->checks |= 4;
    if (machine_service() != 1 || m->services != 2)
        return 4;
    m->checks |= 8;
    if (m->mode == 1) {
        machine_fault();
        if (m->fault_vector != 13 || m->fault_error != 0x30 ||
            m->fault_cs != 8)
            return 5;
        m->checks |= 16;
    }
    if (m->mode == 2)
        m->result ^= 1; /* Self-test: let the caller detect a corrupted checksum. */
    if (m->mode == 4) {
        status = heap_kernel_pm_initialize();
        if (status) return status;
        request_init();
        status = machine_run_program();
        /* Preserve user status before any resumable DOS cleanup can fail. */
        *(volatile u16 *)0x101c = status;
        heap_kernel_client_status(status);
        {
            u16 cleanup = request_cleanup();
            if (cleanup && !status) status = 73;
        }
        return heap_kernel_protected_finish(status);
    }
    if (m->mode == 3)
        return 0x31; /* Explicit kernel-error to DOS-exit propagation control. */
    return 0;
}

/* Compiled ring0 callback for the separate ABI0.5 small privilege fixture. */
u16 kernel_privilege(u16 value)
{
    return (u16)(((value << 3) | (value >> 13)) ^ MAILBOX->seed ^ 0x6b2d);
}
