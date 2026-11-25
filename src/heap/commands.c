#include "commands.h"
static heap_u16 word(const heap_u8 *p) {
  return (heap_u16)((heap_u16)p[0] | ((heap_u16)p[1] << 8));
}
static void put_word(heap_u8 *p, heap_u16 value) {
  p[0] = (heap_u8)(value & 255u);
  p[1] = (heap_u8)(value >> 8);
}
static void put_result(heap_u8 *p, heap_u32 value) {
  put_word(p, (heap_u16)(value & 65535UL));
  put_word(p + 2, (heap_u16)(value >> 16));
}
heap_u16 heap_command(struct heap_command_context *c, heap_u8 *record,
                      heap_u16 length) {
  heap_u16 op, origin, paragraphs, i, status, error, maximum;
  heap_u32 result;
  struct heap_descriptor_result r;
  if (length != HEAP_COMMAND_BYTES || !record)
    return 1;
  if (word(record) != 1u)
    return 2;
  op = word(record + 2);
  if (op > 3u)
    return 3;
  if (word(record + 4) || word(record + 10))
    return 4;
  for (i = 12; i != 32; ++i)
    if (record[i])
      return 4;
  origin = word(record + 6);
  paragraphs = word(record + 8);
  if ((op != 2u && origin) || (op != 1u && paragraphs) ||
      (op == 1u && (!paragraphs || paragraphs > HEAP_PARAGRAPHS)))
    return 8;
  if (!c || !c->heap || !c->descriptors || !c->descriptors->install ||
      !c->descriptors->invalidate)
    return 8;
  if (c->terminal)
    return HEAP_COMMAND_TERMINAL;
  c->descriptor_error = c->cleanup_error = c->cleanup_reference = 0;
  result = 0;
  status = error = 0;
  if (!op)
    result = 15;
  else if (op == 3u)
    result = heap_maximum(c->heap);
  else if (op == 1u) {
    heap_descriptor_allocate(c->heap, paragraphs, c->descriptors, &maximum, &r);
    if (!r.error && !r.cleanup_error)
      result = r.selector;
    else {
      c->descriptor_error = r.error;
      c->cleanup_error = r.cleanup_error;
      error = r.error;
      status = 9;
      if (r.error == HEAP_NOMEM && paragraphs > maximum)
        result = maximum;
      if (r.cleanup_error || r.reference) {
        c->terminal = 1;
        c->cleanup_reference = r.reference;
        status = HEAP_COMMAND_TERMINAL;
      }
    }
  } else {
    error = heap_release_preflight(c->heap, origin, &c->client);
    if (error)
      status = 9;
    else {
      heap_descriptor_release(c->heap, origin, c->descriptors, &r);
      if (r.error || r.cleanup_error) {
        c->descriptor_error = r.error;
        c->cleanup_error = r.cleanup_error ? r.cleanup_error : r.error;
        c->cleanup_reference = r.reference;
        c->terminal = 1;
        status = HEAP_COMMAND_TERMINAL;
        error = r.error;
      }
    }
  }
  put_word(record + 10, c->cleanup_error);
  put_result(record + 24, result);
  put_word(record + 28, error);
  return status;
}
