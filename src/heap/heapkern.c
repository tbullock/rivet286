#include "heapkern.h"
#ifndef PMTEST_HEAP_KERNEL_HOST
static void *heap_kernel_address(heap_u16 offset) { return (void *)offset; }
#endif
static struct heap_kernel_flags *flags(void) {
    return (struct heap_kernel_flags *)heap_kernel_address(HEAP_KERNEL_FLAGS);
}
static struct heap_state *heap(void) {
    return (struct heap_state *)heap_kernel_address(HEAP_KERNEL_STATE);
}
static struct xms_state *xms(void) {
    return (struct xms_state *)heap_kernel_address(HEAP_KERNEL_XMS);
}
static struct heap_command_context *command(void) {
    return (struct heap_command_context *)heap_kernel_address(HEAP_KERNEL_CONTEXT);
}
static struct heap_descriptor_ops *operations(void) {
    return (struct heap_descriptor_ops *)heap_kernel_address(HEAP_KERNEL_OPERATIONS);
}
static heap_u16 first(heap_u16 status) {
    if (status && !flags()->first_status) flags()->first_status=status;
    return flags()->first_status;
}
static heap_u16 captured(heap_u16 offset) {
    const heap_u8 *p=(const heap_u8 *)heap_kernel_address(offset);
    return (heap_u16)((heap_u16)p[0]|((heap_u16)p[1]<<8));
}
static void diagnostics(heap_u16 error,heap_u16 cleanup) {
    if (error && !flags()->descriptor_error) flags()->descriptor_error=error;
    if (cleanup && !flags()->descriptor_cleanup) flags()->descriptor_cleanup=cleanup;
}
heap_u16 heap_kernel_real_prepare(void) {
    struct heap_kernel_flags *f=flags();
    f->prepared=f->initialized=f->stopped=f->first_status=0;
    f->descriptor_error=f->descriptor_cleanup=f->pic_cleanup=0;
    xms_initialize(xms());
    if (xms_prepare(xms())) return first(HEAP_KERNEL_FAILURE);
    f->prepared=1;return 0;
}
heap_u16 heap_kernel_pm_initialize(void) {
    struct heap_command_context *c=command();
    struct heap_descriptor_ops *ops=operations();
    if (!flags()->prepared || flags()->initialized || flags()->stopped || flags()->first_status ||
        !xms()->ready || xms()->primary.kind || xms()->block!=XMS_HELD ||
        xms()->lock!=XMS_HELD || xms()->local!=XMS_HELD) return first(HEAP_KERNEL_FAILURE);
    if (heap_initialize(heap(),xms()->physical_base)) return first(HEAP_KERNEL_FAILURE);
    ops->install=heap_gdt_install;ops->invalidate=heap_gdt_invalidate;ops->context=0;
    c->heap=heap();c->descriptors=ops;
    c->client.ds=c->client.es=c->client.ss=0;
    c->terminal=c->cleanup_reference=c->descriptor_error=c->cleanup_error=0;
    flags()->initialized=1;return 0;
}
heap_u16 heap_kernel_request(void) {
    struct heap_command_context *c=command();
    heap_u16 status;
    if (!flags()->initialized || flags()->stopped || flags()->first_status || !xms()->ready) {
        (void)first(HEAP_KERNEL_FAILURE);return HEAP_COMMAND_TERMINAL;
    }
    c->client.ds=captured(0x1038);c->client.ss=captured(0x103a);c->client.es=captured(0x103c);
    status=heap_command(c,(heap_u8 *)heap_kernel_address(0x6000),HEAP_COMMAND_BYTES);
    if (status==HEAP_COMMAND_TERMINAL) {
        diagnostics(c->descriptor_error,c->cleanup_error);
        (void)first(HEAP_KERNEL_FAILURE);
    }
    return status;
}
void heap_kernel_client_status(heap_u16 status) {
    (void)first(status);
}
heap_u16 heap_kernel_protected_finish(heap_u16 client_status) {
    struct heap_descriptor_result r;
    heap_u16 i,origin;
    (void)first(client_status);flags()->stopped=1;
    if (!flags()->initialized) return flags()->first_status;
    for(i=0;i!=HEAP_RECORDS;++i) {
        if (!heap()->records[i].phase) continue;
        if (heap()->records[i].phase==1u)
            heap_descriptor_abort(heap(),(heap_u16)(i+1u),operations(),&r);
        else {
            origin=(heap_u16)(0x100u+heap()->records[i].logical_first*16u);
            heap_descriptor_release(heap(),origin,operations(),&r);
        }
        if (r.error || r.cleanup_error) {
            diagnostics(r.error,r.cleanup_error ? r.cleanup_error : r.error);
            (void)first(HEAP_KERNEL_FAILURE);
        }
    }
    if (command()->cleanup_reference &&
        !heap()->records[command()->cleanup_reference-1u].phase) command()->cleanup_reference=0;
    return flags()->first_status;
}
heap_u16 heap_kernel_real_reenter(void) {
    if (!flags()->prepared || flags()->stopped || xms_reenter(xms())) {
        (void)first(HEAP_KERNEL_FAILURE);return HEAP_KERNEL_FAILURE;
    }
    return 0;
}
heap_u16 heap_kernel_real_cleanup(void) {
    heap_u16 error;
    /* Assembly guarantees the client is stopped and DOS tables restored,
       including failed reentry that cannot reach protected_finish again. */
    flags()->stopped=1;
    error=xms_cleanup(xms());flags()->prepared=0;
    if (error) { (void)first(HEAP_KERNEL_FAILURE);return HEAP_KERNEL_FAILURE; }
    return 0;
}
