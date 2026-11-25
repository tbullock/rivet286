#include "startnat.h"
static startup_u16 sn_word(const startup_u8 *p) {
  return (startup_u16)((startup_u16)p[0] | ((startup_u16)p[1] << 8));
}
static startup_u32 sn_dword(const startup_u8 *p) {
  return (startup_u32)sn_word(p) | ((startup_u32)sn_word(p + 2) << 16);
}
static void sn_result(startup_u8 *p, startup_u32 value) {
  p[0] = (startup_u8)value;
  p[1] = (startup_u8)(value >> 8);
  p[2] = (startup_u8)(value >> 16);
  p[3] = (startup_u8)(value >> 24);
}
static startup_u16 sn_owner_status(startup_u16 status) {
  return (status == STARTUP_UNAVAILABLE || status == STARTUP_BINDING)
             ? SN_BINDING
             : SN_ARGUMENT;
}
startup_u16 startup_native_request(const struct startup_state *state,
                                   const struct startup_native_ops *ops,
                                   startup_u8 *record, startup_u16 length) {
  startup_u16 op, item, binding, kind, size, status, done = 0, chunk;
  startup_u32 pointer, count;
  if (length != SN_BYTES)
    return SN_LENGTH;
  if (!record)
    return SN_ARGUMENT;
  if (sn_word(record) != 1)
    return SN_VERSION;
  op = sn_word(record + 2);
  if (op > 1)
    return SN_OPERATION;
  if (sn_word(record + 10) || sn_dword(record + 20) || sn_dword(record + 24) ||
      sn_dword(record + 28))
    return SN_RESERVED;
  item = sn_word(record + 4);
  binding = sn_word(record + 6);
  kind = sn_word(record + 8);
  pointer = sn_dword(record + 12);
  count = sn_dword(record + 16);
  if (!op && (kind || pointer || count))
    return SN_ARGUMENT;
  status = startup_size(state, binding, item, &size);
  if (status)
    return sn_owner_status(status);
  if (!op) {
    sn_result(record + 24, size);
    return 0;
  }
  if ((kind != 1 && kind != 2) || count != size)
    return SN_ARGUMENT;
  /* Portable domain/span checks only. Descriptor rights, model admission and
     full LIVE owner validation are the preflight callback's responsibility. */
  if (kind == 1) {
    if (count > 65536UL - (startup_u32)(startup_u16)pointer)
      return SN_EXTENT;
  } else if (pointer < 0x10000UL || pointer >= 0x10000000UL ||
             count > 0x10000000UL - pointer)
    return SN_EXTENT;
  if (!ops || !ops->bounce || ops->bounce_bytes < 512 || !ops->preflight ||
      !ops->copy)
    return SN_ARGUMENT;
  status = ops->preflight(ops->context, kind, pointer, count);
  if (status)
    return status <= SN_ARGUMENT ? status : SN_ARGUMENT;
  while (done < size) {
    chunk = (startup_u16)(size - done > 512 ? 512 : size - done);
    status = startup_read(state, binding, item, done, ops->bounce, chunk);
    if (status)
      return sn_owner_status(status);
    status = ops->copy(ops->context, kind, pointer, done, ops->bounce, chunk);
    if (status)
      return status <= SN_ARGUMENT ? status : SN_ARGUMENT;
    done = (startup_u16)(done + chunk);
    sn_result(record + 24, done);
  }
  return 0;
}
