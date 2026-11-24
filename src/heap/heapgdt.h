#ifndef PMTEST_HEAP_GDT_H
#define PMTEST_HEAP_GDT_H
/* Only the legacy small-model kernel may link the prepared assembly boundary.
   Host callbacks use ordinary C struct layout and do not prove this target ABI. */
#include "heap.h"
#include "heapkern.h"
#include "xms.h"
#include <stddef.h>
#ifdef _MSC_VER
/* The real-mode diagnostic table reads these target word offsets directly. */
typedef char heap_xms_report_layout[(sizeof(struct xms_state)==46 &&
    offsetof(struct xms_state,version)==4 &&
    offsetof(struct xms_state,block)==14 &&
    offsetof(struct xms_state,lock)==16 &&
    offsetof(struct xms_state,local)==18 &&
    offsetof(struct xms_state,physical_base)==22 &&
    offsetof(struct xms_state,primary)==26 &&
    offsetof(struct xms_state,cleanup)==36 &&
    sizeof(struct xms_failure)==10)?1:-1];
typedef char heap_flag_report_layout[(offsetof(struct heap_kernel_flags,first_status)==6 &&
    offsetof(struct heap_kernel_flags,descriptor_error)==8 &&
    offsetof(struct heap_kernel_flags,descriptor_cleanup)==10 &&
    offsetof(struct heap_kernel_flags,pic_cleanup)==12)?1:-1];
typedef char heap_alias_assembly_layout[(sizeof(struct heap_alias)==10 &&
    offsetof(struct heap_alias,physical_base)==0 &&
    offsetof(struct heap_alias,selector)==4 &&
    offsetof(struct heap_alias,limit)==6 &&
    offsetof(struct heap_alias,present)==8)?1:-1];
#endif
heap_u16 heap_gdt_install(void *,const struct heap_alias *);
heap_u16 heap_gdt_invalidate(void *,heap_u16);
#endif
