#ifndef PMTEST_HEAP_KERNEL_H
#define PMTEST_HEAP_KERNEL_H
#include "commands.h"
#include "xms.h"
#define HEAP_KERNEL_STATE 0x8000u
#define HEAP_KERNEL_XMS 0x9500u
#define HEAP_KERNEL_RAW_ENTRY 0x9540u
#define HEAP_KERNEL_CONTEXT 0x9560u
#define HEAP_KERNEL_OPERATIONS 0x9580u
#define HEAP_KERNEL_FLAGS 0x95a0u
#define HEAP_KERNEL_FAILURE 74u
struct heap_kernel_flags {
    heap_u16 prepared,initialized,stopped,first_status;
    heap_u16 descriptor_error,descriptor_cleanup,pic_cleanup;
};
typedef char heap_kernel_state_region[(sizeof(struct heap_state)<=0x14d4u)?1:-1];
typedef char heap_kernel_xms_region[(sizeof(struct xms_state)<=0x40u)?1:-1];
typedef char heap_kernel_context_region[(sizeof(struct heap_command_context)<=0x20u)?1:-1];
typedef char heap_kernel_operations_region[(sizeof(struct heap_descriptor_ops)<=0x20u)?1:-1];
typedef char heap_kernel_flags_region[(sizeof(struct heap_kernel_flags)<=0x20u)?1:-1];
typedef char heap_kernel_regions_disjoint[(HEAP_KERNEL_STATE+0x14d4u<=HEAP_KERNEL_XMS &&
    HEAP_KERNEL_XMS+0x40u<=HEAP_KERNEL_RAW_ENTRY && HEAP_KERNEL_RAW_ENTRY+4u<=HEAP_KERNEL_CONTEXT &&
    HEAP_KERNEL_CONTEXT+0x20u<=HEAP_KERNEL_OPERATIONS &&
    HEAP_KERNEL_OPERATIONS+0x20u<=HEAP_KERNEL_FLAGS && HEAP_KERNEL_FLAGS+0x20u<=0x95c0u)?1:-1];
/* Only the host model replaces near fixed offsets with an aligned 64 KiB map.
   Target DS must address the host map, and real-mode calls require DS=SS=CS.
   These routines have no static data or structure-valued returns. */
#ifdef PMTEST_HEAP_KERNEL_HOST
void *heap_kernel_address(heap_u16 offset);
#endif
heap_u16 heap_gdt_install(void *context,const struct heap_alias *alias);
heap_u16 heap_gdt_invalidate(void *context,heap_u16 selector);
/* real_prepare is for a fresh owner only, not reinitialization of live state. */
heap_u16 heap_kernel_real_prepare(void);
heap_u16 heap_kernel_pm_initialize(void);
heap_u16 heap_kernel_request(void);
/* Record the stopped program result before DOS file cleanup may reenter.
   This records diagnostics only; it neither changes phase nor calls XMS. */
void heap_kernel_client_status(heap_u16 status);
/* Client must already be stopped. All records are attempted even after errors.
   Failed descriptor cleanup forbids client resumption; metadata remains owned. */
heap_u16 heap_kernel_protected_finish(heap_u16 client_status);
heap_u16 heap_kernel_real_reenter(void);
/* Client stopped and original DOS tables restored before this real-mode call. */
heap_u16 heap_kernel_real_cleanup(void);
#endif
