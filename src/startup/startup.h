#ifndef PMTEST_STARTUP_H
#define PMTEST_STARTUP_H
#include <limits.h>
#include <stddef.h>
#ifdef _MSC_VER
typedef unsigned short startup_u16;
typedef unsigned long startup_u32;
#else
#include <stdint.h>
typedef uint16_t startup_u16;
typedef uint32_t startup_u32;
#endif
typedef unsigned char startup_u8;
typedef char startup_widths[(CHAR_BIT == 8 && sizeof(startup_u16) == 2 &&
                             sizeof(startup_u32) == 4)
                                ? 1
                                : -1];
#define STARTUP_OK 0u
#define STARTUP_UNAVAILABLE 1u
#define STARTUP_BINDING 2u
#define STARTUP_ITEM 3u
#define STARTUP_SIZE 4u
#define STARTUP_SOURCE 5u
#define STARTUP_ARGUMENT 6u
#define STARTUP_ENV 1u
#define STARTUP_COMMAND 2u
#define STARTUP_EXECUTABLE 3u
struct startup_meta {
  startup_u16 binding, prepared, ready, env_size, cmd_size, exe_size;
  startup_u16 reserved[26];
};
struct startup_state {
  struct startup_meta meta;
  startup_u8 env[2048], command[128], executable[128];
};
typedef char startup_meta_size[(sizeof(struct startup_meta) == 64) ? 1 : -1];
typedef char
    startup_env_offset[(offsetof(struct startup_state, env) == 0x40) ? 1 : -1];
typedef char
    startup_cmd_offset[(offsetof(struct startup_state, command) == 0x840) ? 1
                                                                          : -1];
typedef char startup_exe_offset
    [(offsetof(struct startup_state, executable) == 0x8c0) ? 1 : -1];
typedef char
    startup_state_size[(sizeof(struct startup_state) == 0x940) ? 1 : -1];
/* Return0 on a successful physical byte read; never retain callback/context.
   Call only before PE. Input arrays are separate from the destination state. */
typedef startup_u16 (*startup_reader)(void *context, startup_u32 address,
                                      startup_u8 *value);
startup_u16 startup_prepare(struct startup_state *state, startup_u16 binding,
                            const startup_u8 *filename,
                            startup_u16 filename_size,
                            const startup_u8 *command, startup_u16 command_size,
                            startup_u16 psp, startup_reader reader,
                            void *context);
startup_u16 startup_publish(struct startup_state *state, startup_u16 binding);
void startup_invalidate(struct startup_state *state);
startup_u16 startup_size(const struct startup_state *state, startup_u16 binding,
                         startup_u16 item, startup_u16 *size);
/* Whole item only, including final NUL(s). Failure does not write destination.
   Destination storage/physical rights preflight belongs to the future adapter.
 */
startup_u16 startup_copy(const struct startup_state *state, startup_u16 binding,
                         startup_u16 item, startup_u8 *destination,
                         startup_u16 size);
/* Private adapter helper: positive item-relative slice<=512, no publication
   mutation. This is not a partial public COPY operation. */
startup_u16 startup_read(const struct startup_state *state, startup_u16 binding,
                         startup_u16 item, startup_u16 offset,
                         startup_u8 *destination, startup_u16 count);
#endif
