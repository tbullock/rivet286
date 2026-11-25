#ifndef PMTEST_HEAP_RANGES_H
#define PMTEST_HEAP_RANGES_H
#include "heap.h"
/* Check that a nonempty logical byte range fits in one live allocation. */
heap_u16 heap_buffer_extent(const struct heap_state *, heap_u32, heap_u32);
#endif
