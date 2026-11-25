#ifndef PMTEST_XMS_DRAFT_H
#define PMTEST_XMS_DRAFT_H
#include <limits.h>
#ifdef _MSC_VER
typedef unsigned short xms_u16;
typedef unsigned long xms_u32;
#else
#include <stdint.h>
typedef uint16_t xms_u16;
typedef uint32_t xms_u32;
#endif
typedef char xms_widths
    [(CHAR_BIT == 8 && sizeof(xms_u16) == 2 && sizeof(xms_u32) == 4) ? 1 : -1];
#define XMS_NONE 0u
#define XMS_HELD 1u
#define XMS_UNKNOWN 2u
#define XMS_OK 0u
#define XMS_STATE 1u
#define XMS_DISCOVERY 2u
#define XMS_VERSION 3u
#define XMS_DRIVER 4u
#define XMS_REPLY 5u
#define XMS_CAPACITY 6u
#define XMS_PHYSICAL 7u
#define XMS_A20_OFF 8u
#define XMS_UNCERTAIN 9u
#define XMS_CALLER 10u
struct xms_regs {
  xms_u16 ax, bx, dx;
};
typedef char xms_packet_width[(sizeof(struct xms_regs) == 6) ? 1 : -1];
struct xms_failure {
  xms_u16 operation, kind;
  struct xms_regs raw;
};
struct xms_state {
  xms_u16 started, ready, version, revision, largest_kib, total_kib, handle;
  xms_u16 block, lock, local, initial_a20;
  xms_u32 physical_base;
  struct xms_failure primary, cleanup;
};
/* Raw real-mode adapter owns the resident far pointer, not the C manager.
   FFFF performs discovery (AX80 success); other function values are AH.
   argument is DX input; results are raw AX/BX/DX. No CF status contract. */
void xms_raw(xms_u16 function, xms_u16 argument, struct xms_regs *result);
void xms_initialize(struct xms_state *);
xms_u16 xms_prepare(struct xms_state *);
xms_u16 xms_reenter(struct xms_state *);
xms_u16 xms_cleanup(struct xms_state *);
void xms_caller_error(struct xms_state *, xms_u16, xms_u16);
#endif
