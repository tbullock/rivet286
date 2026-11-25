#ifndef PMTEST_HEAP_DRAFT_H
#define PMTEST_HEAP_DRAFT_H
#include <limits.h>
#ifdef _MSC_VER
typedef unsigned short heap_u16;
typedef unsigned long heap_u32;
#else
#include <stdint.h>
typedef uint16_t heap_u16;
typedef uint32_t heap_u32;
#endif
typedef unsigned char heap_u8;
typedef char heap_byte_width[(CHAR_BIT == 8) ? 1 : -1];
typedef char heap_word_width[(sizeof(heap_u16) == 2) ? 1 : -1];
typedef char heap_dword_width[(sizeof(heap_u32) == 4) ? 1 : -1];
#define HEAP_PARAGRAPHS 32768u
#define HEAP_SLOTS 112u
#define HEAP_RECORDS 112u
#define HEAP_OK 0u
#define HEAP_NOMEM 8u
#define HEAP_BAD_ORIGIN 9u
#define HEAP_BAD_ARGUMENT 87u
struct heap_record {
  heap_u16 phase, logical_first, slots, physical_first, paragraphs;
};
struct heap_state {
  heap_u32 physical_base;
  heap_u8 physical[HEAP_PARAGRAPHS / 8u];
  heap_u8 logical[HEAP_SLOTS];
  struct heap_record records[HEAP_RECORDS];
};
struct heap_alias {
  heap_u32 physical_base;
  heap_u16 selector, limit, present;
};
/* A private record reference is index+1 and remains valid while PENDING/LIVE.
   Commit/abort require PENDING. Abort/release consume the reference; a later
   reuse does not revive old references. None cross the provider boundary. */
heap_u16 heap_initialize(struct heap_state *, heap_u32);
heap_u16 heap_begin(struct heap_state *, heap_u16, heap_u16 *, heap_u16 *);
heap_u16 heap_commit(struct heap_state *, heap_u16, heap_u16 *);
heap_u16 heap_abort(struct heap_state *, heap_u16);
heap_u16 heap_find_live(const struct heap_state *, heap_u16, heap_u16 *);
heap_u16 heap_release(struct heap_state *, heap_u16);
/* Alias lookup accepts a private reference while its record is PENDING/LIVE. */
heap_u16 heap_alias_at(const struct heap_state *, heap_u16, heap_u16,
                       struct heap_alias *);
heap_u16 heap_maximum(const struct heap_state *);
#endif
