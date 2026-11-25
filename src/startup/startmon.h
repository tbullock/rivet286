#ifndef PMTEST_STARTUP_KERNEL_H
#define PMTEST_STARTUP_KERNEL_H
#include "startnat.h"
#define STARTUP_STATE 0x95c0u
#define STARTUP_CONTEXT 0x9f00u
#define STARTUP_FILENAME 0x9f20u
#define STARTUP_FRAME 0x9fa0u
struct startup_context {
  startup_u16 profile, binding, active, exit_requested, exit_status, exit_error;
  startup_u16 prepare_error, publish_error, saved_ss, saved_sp, saved_flags;
  startup_u16 wrapper_result, wrapper_entry, reserved[3];
};
typedef char startup_map_assert[(STARTUP_STATE + sizeof(struct startup_state) ==
                                     STARTUP_CONTEXT &&
                                 sizeof(struct startup_context) == 32 &&
                                 STARTUP_FILENAME == STARTUP_CONTEXT + 32 &&
                                 STARTUP_FILENAME + 128 == STARTUP_FRAME &&
                                 STARTUP_FRAME + 24 <= 0xa000)
                                    ? 1
                                    : -1];
typedef char
    startup_exit_layout[(offsetof(struct startup_context, active) == 4 &&
                         offsetof(struct startup_context, exit_requested) ==
                             6 &&
                         offsetof(struct startup_context, exit_status) == 8 &&
                         offsetof(struct startup_context, exit_error) == 10 &&
                         offsetof(struct startup_context, saved_ss) == 16 &&
                         offsetof(struct startup_context, wrapper_entry) == 24)
                            ? 1
                            : -1];
#ifdef PMTEST_STARTUP_HOST
void *startup_address(startup_u16 offset);
#endif
startup_u16 startup_phys_read(void *context, startup_u32 address,
                              startup_u8 *value);
startup_u16 startup_real_cs(void);
/* Parser result0 is unchanged legacy profile;1 selects /R;67 malformed /R. */
startup_u16 startup_parse(const startup_u8 *tail, startup_u16 length,
                          startup_u8 *filename, startup_u16 *name_size,
                          startup_u16 *command_offset,
                          startup_u16 *command_size);
startup_u16 startup_kernel_prepare(void);
startup_u16 startup_kernel_publish(void);
void startup_kernel_drop(void);
startup_u16 startup_kernel_channel(void);
#endif
