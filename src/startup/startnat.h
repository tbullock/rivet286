#ifndef PMTEST_STARTUP_NATIVE_H
#define PMTEST_STARTUP_NATIVE_H
#include "startup.h"
#define SN_BYTES 32u
#define SN_LENGTH 1u
#define SN_VERSION 2u
#define SN_OPERATION 3u
#define SN_RESERVED 4u
#define SN_EXTENT 5u
#define SN_BUSY 6u
#define SN_BINDING 7u
#define SN_ARGUMENT 8u
/* All callbacks return0 on success or a native status1..8 on failure.
   Unknown callback statuses map to argument8; no new public status is exposed.
   The preflight validates the entire writable destination before any copy
   effect. Copy always receives original wire pointer plus a separate byte
   offset. */
typedef startup_u16 (*sn_preflight)(void *context, startup_u16 kind,
                                    startup_u32 pointer, startup_u32 count);
typedef startup_u16 (*sn_copy)(void *context, startup_u16 kind,
                               startup_u32 pointer, startup_u32 offset,
                               const startup_u8 *source, startup_u16 count);
struct startup_native_ops {
  void *context;
  startup_u8 *bounce;
  startup_u16 bounce_bytes;
  sn_preflight preflight;
  sn_copy copy;
};
/* Staged record and bounce must be disjoint from immutable owner storage.
   Scalar status6 is reserved to a future adapter; this owner has no busy state
   or DOS calls (a callback may propagate a closed native status). */
startup_u16 startup_native_request(const struct startup_state *state,
                                   const struct startup_native_ops *ops,
                                   startup_u8 *record, startup_u16 length);
#endif
