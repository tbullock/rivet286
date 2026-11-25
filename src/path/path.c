/* Portable path policy owner; DOS transition remains in the assembly gateway.
 */
#include "path.h"
static pm_word word(const pm_byte *p) {
  return (pm_word)((pm_word)p[0] | ((pm_word)p[1] << 8));
}
static pm_dword dword(const pm_byte *p) {
  return (pm_dword)word(p) | ((pm_dword)word(p + 2) << 16);
}
static void put(pm_byte *p, pm_word v) {
  p[0] = (pm_byte)v;
  p[1] = (pm_byte)(v >> 8);
}
static void result(pm_byte *p, pm_word v) {
  put(p + 24, v);
  put(p + 26, 0);
}
static pm_word name(const pm_byte *p, pm_word n) {
  pm_word i;
  if (n < 2 || n > 128 || p[n - 1])
    return PM_REQUEST_BAD_ARGUMENT;
  for (i = 0; i + 1 < n; ++i)
    if (!p[i])
      return PM_REQUEST_BAD_ARGUMENT;
  return 0;
}
pm_word path_dispatch(pm_byte *record, const struct path_host *host) {
  pm_word op, flags, kind, split, count, status, i, slot = 8, ax = 0, cx = 0,
                                                    bx = 0, dx = 0, output = 0;
  pm_dword pointer, size;
  struct path_dos_result returned;
  if (word(record) != PATH_VERSION && word(record) != 2u)
    return PM_REQUEST_BAD_VERSION;
  if (word(record + 6) || dword(record + 20) || dword(record + 24) ||
      word(record + 28) || word(record + 30))
    return PM_REQUEST_BAD_RESERVED;
  op = word(record + 2);
  flags = word(record + 4);
  kind = word(record + 8);
  split = word(record + 10);
  pointer = dword(record + 12);
  size = dword(record + 16);
  if (op > PATH_EXCLUSIVE)
    return PM_REQUEST_BAD_OPERATION;
  if (op != PATH_RENAME && split)
    return PM_REQUEST_BAD_ARGUMENT;
  if (op == PATH_CWD) {
    if (flags < 1 || flags > 26)
      return PM_REQUEST_BAD_ARGUMENT;
  } else if (op == PATH_SETATTR) {
    if (flags & ~0x27u)
      return PM_REQUEST_BAD_ARGUMENT;
  } else if (flags)
    return PM_REQUEST_BAD_ARGUMENT;
  if (op <= PATH_DRIVE) {
    if (kind || pointer || size)
      return PM_REQUEST_BAD_ARGUMENT;
    if (op == PATH_QUERY) {
      result(record, (pm_word)(word(record) == PATH_VERSION ? 0x7ff : 0x1fff));
      return 0;
    }
  } else {
    if (kind < PM_BUFFER_SELECTOR || kind > PM_BUFFER_LOGICAL || size > 256UL)
      return PM_REQUEST_BAD_ARGUMENT;
    count = (pm_word)size;
    if ((kind == PM_BUFFER_SELECTOR &&
         (pm_dword)(pm_word)pointer + size > 65536UL) ||
        (size && pointer > 0xffffffffUL - (size - 1UL)))
      return PM_REQUEST_BAD_EXTENT;
    if (op == PATH_CWD) {
      if (count != 64)
        return PM_REQUEST_BAD_ARGUMENT;
      output = 1;
    } else if (op == PATH_RENAME) {
      if (split < 2 || split > 128 || count < split + 2 || count - split > 128)
        return PM_REQUEST_BAD_ARGUMENT;
    } else if (count < 2 || count > 128)
      return PM_REQUEST_BAD_ARGUMENT;
    /* Every byte and direction is checked before copying or DOS effects. */
    status = host->access(host->context, kind, pointer, count, output);
    if (status)
      return status;
    if (!output) {
      status = host->copy(host->context, kind, pointer, count, 0);
      if (status)
        return status;
      if (op == PATH_RENAME) {
        status = name(host->bounce, split);
        if (!status)
          status = name(host->bounce + split, (pm_word)(count - split));
      } else
        status = name(host->bounce, count);
      if (status)
        return status;
    }
  }
  switch (op) {
  case PATH_DRIVE:
    ax = 0x1900;
    break;
  case PATH_CWD:
    ax = 0x4700;
    dx = flags;
    for (i = 0; i < 64; ++i)
      host->bounce[i] = 0xa5;
    break;
  case PATH_GETATTR:
    ax = 0x4300;
    dx = 0x6200;
    break;
  case PATH_SETATTR:
    ax = 0x4301;
    cx = flags;
    dx = 0x6200;
    break;
  case PATH_DELETE:
    ax = 0x4100;
    dx = 0x6200;
    break;
  case PATH_MKDIR:
    ax = 0x3900;
    dx = 0x6200;
    break;
  case PATH_RMDIR:
    ax = 0x3a00;
    dx = 0x6200;
    break;
  case PATH_CHDIR:
    ax = 0x3b00;
    dx = 0x6200;
    break;
  case PATH_RENAME:
    ax = 0x5600;
    bx = split;
    cx = count;
    dx = 0x6200;
    break;
  case PATH_EXCLUSIVE:
    /* Reserve by finding the shared slot before the atomic DOS5B call.
       Serialized service ownership excludes another opener until adoption. */
    for (slot = 0; slot < 8 && host->handles[slot] != 0xffff; ++slot) {
    }
    if (slot == 8)
      return PM_REQUEST_HANDLE_FULL;
    ax = 0x5b00;
    dx = 0x6200;
    break;
  default:
    return PM_REQUEST_BAD_OPERATION;
  }
  status = host->dos(host->context, ax, bx, cx, dx, &returned);
  if (status)
    return status;
  if (op == PATH_DRIVE) {
    /* AH19 has no CF result. AL is zero-based drive only. */
    i = (pm_word)(returned.ax & 255u);
    if (i > 25)
      return PM_REQUEST_BAD_ARGUMENT;
    result(record, (pm_word)(i + 1));
    return 0;
  }
  if (returned.flags & 1) {
    put(record + 28, returned.ax);
    return PM_REQUEST_DOS_FAILURE;
  }
  if (op == PATH_EXCLUSIVE) {
    /* Publish raw handle in the existing owner before response copying. */
    if (returned.ax == 0xffff)
      return PATH_TERMINAL;
    host->handles[slot] = returned.ax;
    result(record, (pm_word)(slot + 1));
  } else if (op == PATH_GETATTR)
    result(record, returned.cx);
  else if (op == PATH_CWD) {
    for (i = 0; i < 64 && host->bounce[i]; ++i) {
    }
    if (i == 64)
      return PM_REQUEST_BAD_ARGUMENT;
    /* Whole64-byte preflight already happened; publish only exact ASCIIZ. */
    /* On copy failure the original zero result stays zero: no successful
       byte count or DOS error is fabricated after the pure CWD query. */
    status = host->copy(host->context, kind, pointer, (pm_word)(i + 1), 1);
    if (status)
      return status;
    result(record, (pm_word)(i + 1));
  }
  return 0;
}
