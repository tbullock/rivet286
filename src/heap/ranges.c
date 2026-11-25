#include "ranges.h"
heap_u16 heap_buffer_extent(const struct heap_state *s, heap_u32 address,
                            heap_u32 count) {
  heap_u16 i;
  heap_u32 origin, bytes, offset;
  const struct heap_record *r;
  if (!s || !count || address < 0x100000UL || address >= 0x800000UL)
    return HEAP_BAD_ARGUMENT;
  if (count > (heap_u32)0x800000u - address)
    return HEAP_BAD_ARGUMENT;
  for (i = 0; i != HEAP_RECORDS; ++i) {
    r = &s->records[i];
    if (r->phase != 2u)
      continue;
    if (r->logical_first >= HEAP_SLOTS || !r->paragraphs ||
        r->paragraphs > HEAP_PARAGRAPHS || !r->slots ||
        r->slots > HEAP_SLOTS - r->logical_first ||
        r->slots != (heap_u16)((r->paragraphs + 4095u) / 4096u))
      return HEAP_BAD_ARGUMENT;
    origin = ((heap_u32)0x100u + (heap_u32)r->logical_first * (heap_u32)16u)
             << 12;
    bytes = (heap_u32)r->paragraphs * (heap_u32)16u;
    if (address < origin)
      continue;
    offset = address - origin;
    if (offset < bytes)
      return count <= bytes - offset ? HEAP_OK : HEAP_BAD_ARGUMENT;
  }
  return HEAP_BAD_ARGUMENT;
}
