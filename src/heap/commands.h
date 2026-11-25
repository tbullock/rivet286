#ifndef PMTEST_HEAP_COMMANDS_H
#define PMTEST_HEAP_COMMANDS_H
#include "descr.h"
#include "segments.h"
#define HEAP_COMMAND_BYTES 32u
#define HEAP_COMMAND_TERMINAL 11u
/* Native entry supplies actual captured segments. A terminal result means the
   stopped client MUST NOT resume: even retained LIVE ownership can have holes.
   cleanup_reference is private metadata, never a client selector. Cleanup and
   descriptor diagnostics remain distinct. Terminal diagnostics are sticky;
   nonterminal command diagnostics describe the most recent accepted request. */
struct heap_command_context {
  struct heap_state *heap;
  const struct heap_descriptor_ops *descriptors;
  struct heap_client_segments client;
  heap_u16 terminal, cleanup_reference, descriptor_error, cleanup_error;
};
/* Caller initializes terminal/reference/diagnostics to zero once per owner.
   Decode is little endian and alignment independent. A malformed request does
   not change the record, context, allocator or descriptors. No native entry,
   GDT publication, XMS lifetime or caller-buffer copying is implemented here.
 */
heap_u16 heap_command(struct heap_command_context *, heap_u8 *, heap_u16);
#endif
