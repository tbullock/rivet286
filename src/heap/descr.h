#ifndef PMTEST_HEAP_DESCRIPTOR_DRAFT_H
#define PMTEST_HEAP_DESCRIPTOR_DRAFT_H
#include "heap.h"
struct heap_descriptor_ops {
    heap_u16 (*install)(void *, const struct heap_alias *);
    heap_u16 (*invalidate)(void *, heap_u16);
    void *context;
};
struct heap_descriptor_result {
    heap_u16 error, cleanup_error, reference, selector;
};
/* Callbacks may have effects before returning an error. The caller serializes
   metadata mutation throughout this private adapter; callbacks must not reenter
   the allocator. Failed release can remove some aliases: keep the client
   stopped until cleanup succeeds or terminal teardown. Retained ownership
   does not promise continued accessibility. No GDT write or public ABI is
   implemented here. Results use caller-owned output objects: legacy MSC may
   allocate hidden static storage for a structure-valued function return. */
void heap_descriptor_allocate(struct heap_state *,
    heap_u16, const struct heap_descriptor_ops *, heap_u16 *,
    struct heap_descriptor_result *);
void heap_descriptor_abort(struct heap_state *,
    heap_u16, const struct heap_descriptor_ops *, struct heap_descriptor_result *);
void heap_descriptor_release(struct heap_state *,
    heap_u16, const struct heap_descriptor_ops *, struct heap_descriptor_result *);
#endif
