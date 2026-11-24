#ifndef PMTEST_HEAP_SEGMENTS_H
#define PMTEST_HEAP_SEGMENTS_H
#include "heap.h"
#define HEAP_ACTIVE_SEGMENT 5u
struct heap_client_segments { heap_u16 ds,es,ss; };
/* Use actual native-entry captures, never a client-supplied claim. Call before
   the first descriptor invalidation. Failure leaves the allocation unchanged.
   This does not replace stopped-client cleanup after partial invalidation. */
heap_u16 heap_release_preflight(const struct heap_state *,heap_u16,
    const struct heap_client_segments *);
#endif
