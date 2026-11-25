#ifndef PMTEST_MAILBOX_H
#define PMTEST_MAILBOX_H
typedef unsigned short u16;
typedef unsigned char u8;
struct mailbox {
  u16 magic, mode, seed, result, checks;
  u16 fault_vector, fault_error, fault_ip, fault_cs;
  u16 resumed, start_msw, end_msw, services, memory_sum;
};
typedef char word_is_16[(sizeof(u16) == 2) ? 1 : -1];
typedef char mailbox_is_28[(sizeof(struct mailbox) == 28) ? 1 : -1];
#define MAILBOX ((volatile struct mailbox *)0x1000)
u16 machine_msw(void);
u16 machine_far_call(void);
u16 machine_service(void);
void machine_fault(void);
u16 machine_run_program(void);
#endif
