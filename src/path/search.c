/* DOS directory search records and handle validation. */
#include "search.h"
static pm_word sw(const pm_byte *p) {
  return (pm_word)((pm_word)p[0] | ((pm_word)p[1] << 8));
}
static pm_dword sd(const pm_byte *p) {
  return (pm_dword)sw(p) | ((pm_dword)sw(p + 2) << 16);
}
static void sp(pm_byte *p, pm_word n) {
  p[0] = (pm_byte)n;
  p[1] = (pm_byte)(n >> 8);
}
static void sl(pm_byte *p, pm_dword n) {
  sp(p, (pm_word)n);
  sp(p + 2, (pm_word)(n >> 16));
}
#ifdef PMTEST_SEARCH_HOST_MODEL
void search_initialize(pm_byte *s) {
  pm_word i;
  for (i = 0; i < SEARCH_STATE_BYTES; ++i)
    s[i] = 0;
}
void search_invalidate(pm_byte *s) {
  pm_dword generation = sd(s + 4);
  search_initialize(s);
  sl(s + 4, generation);
}
void search_launch(pm_byte *s, pm_word binding) {
  search_invalidate(s);
  sp(s, binding);
}
static pm_word equal(struct search_pointer a, struct search_pointer b) {
  return (pm_word)(a.offset == b.offset && a.segment == b.segment);
}
static void increment(pm_byte *p) {
  pm_word n = sw(p);
  if (n != 0xffffu)
    sp(p, (pm_word)(n + 1u));
}
static pm_word restore(const struct search_host *h,
                       struct search_pointer saved) {
  pm_word attempt;
  /* Bounded2 attempts. No later path may return to DOS if both fail. */
  for (attempt = 0; attempt < 2; ++attempt) {
    h->set_dta(h->context, saved);
    if (equal(h->get_dta(h->context), saved)) {
      increment(h->state + 62);
      return 0;
    }
  }
  return SEARCH_FAILSTOP;
}
pm_word search_transaction_model(const struct search_host *h, pm_word op,
                                 pm_word attrs,
                                 struct search_dos_result *result) {
  struct search_pointer saved;
  pm_word status;
  saved = h->get_dta(h->context);
  sp(h->state + 52, saved.offset);
  sp(h->state + 54, saved.segment);
  sp(h->state + 56, 0);
  sp(h->state + 58, 0);
  h->set_dta(h->context, h->private_dta);
  if (!equal(h->get_dta(h->context), h->private_dta)) {
    status = restore(h, saved);
    return status ? SEARCH_FAILSTOP_INSTALL : PM_REQUEST_BUSY;
  }
  increment(h->state + 60);
  h->find(h->context, (pm_word)(op == SEARCH_FIRST ? 0x4e00 : 0x4f00), attrs,
          h->bounce, h->state + 8, result);
  /* Persist raw result and original pointer before any restore calls. */
  sp(h->state + 56, result->ax);
  sp(h->state + 58, result->flags);
  status = restore(h, saved);
  return status;
}
#endif
pm_word search_dispatch(pm_byte *r, const struct search_host *h) {
  pm_word op, attrs, kind, split, count, status, i;
  pm_dword ptr, size, token, generation;
  struct search_dos_result returned;
  if (sw(r) != 2)
    return PM_REQUEST_BAD_VERSION;
  if (sd(r + 24) || sw(r + 28) || sw(r + 30))
    return PM_REQUEST_BAD_RESERVED;
  op = sw(r + 2);
  attrs = sw(r + 4);
  kind = sw(r + 8);
  split = sw(r + 10);
  ptr = sd(r + 12);
  size = sd(r + 16);
  token = sd(r + 20);
  if (op == 0) {
    if (attrs || sw(r + 6) || kind || split || ptr || size || token)
      return PM_REQUEST_BAD_ARGUMENT;
    sl(r + 24, 0x1fffUL);
    return 0;
  }
  if (op != SEARCH_FIRST && op != SEARCH_NEXT)
    return PM_REQUEST_BAD_OPERATION;
  if (h->profile != 1 || h->active != 1 || h->ready != 1 || !h->binding ||
      sw(r + 6) != h->binding || sw(h->state) != h->binding)
    return PM_REQUEST_BAD_HANDLE;
  if (kind < PM_BUFFER_SELECTOR || kind > PM_BUFFER_LOGICAL || size > 152UL)
    return PM_REQUEST_BAD_ARGUMENT;
  count = (pm_word)size;
  if ((kind == PM_BUFFER_SELECTOR && (pm_dword)(pm_word)ptr + size > 65536UL) ||
      (size && ptr > 0xffffffffUL - (size - 1UL)))
    return PM_REQUEST_BAD_EXTENT;
  if (op == SEARCH_FIRST) {
    if (token || attrs & ~0x37u || split < 2 || split > 128 ||
        count != split + 24u)
      return PM_REQUEST_BAD_ARGUMENT;
  } else if (attrs || split || count != 24 || !token || !sw(h->state + 2) ||
             token != sd(h->state + 4))
    return PM_REQUEST_BAD_HANDLE;
  /* Both directions over whole compound FIRST extent before advancing DOS. */
  status = h->access(h->context, kind, ptr, count, 1);
  if (status)
    return status;
  if (op == SEARCH_FIRST) {
    status = h->access(h->context, kind, ptr, count, 0);
    if (status)
      return status;
    status = h->copy(h->context, kind, ptr, split, 0);
    if (status)
      return status;
    if (h->bounce[split - 1])
      return PM_REQUEST_BAD_ARGUMENT;
    for (i = 0; i + 1u < split; ++i)
      if (!h->bounce[i])
        return PM_REQUEST_BAD_ARGUMENT;
    generation = sd(h->state + 4);
    if (generation == 0xffffffffUL)
      return PM_REQUEST_BAD_ARGUMENT;
    sp(h->state + 2, 0);
    for (i = 8; i < 51; ++i)
      h->state[i] = 0;
    ++generation;
    sl(h->state + 4, generation);
  } else
    generation = token;
  status = h->transaction(h, op, attrs, &returned);
  if (status == SEARCH_FAILSTOP || status == SEARCH_FAILSTOP_INSTALL)
    return status; /* Actual gateway never returns. */
  if (status) {
    sp(h->state + 2, 0);
    return status;
  }
  if (returned.flags & 1u) {
    sp(h->state + 2, 0);
    sp(r + 28, returned.ax);
    return PM_REQUEST_DOS_FAILURE;
  }
  /* DOS43-byte DTA offsets21/22/24/26/30. No reserved bytes escape. */
  for (i = 0; i < 13 && h->state[38 + i]; ++i) {
  }
  if (i == 13) {
    sp(h->state + 2, 0);
    return SEARCH_TERMINAL;
  }
  for (i = 0; i < 24; ++i)
    h->bounce[i] = 0;
  sp(h->bounce, h->state[29]);
  sp(h->bounce + 2, sw(h->state + 30));
  sp(h->bounce + 4, sw(h->state + 32));
  sl(h->bounce + 6, sd(h->state + 34));
  for (i = 0; i < 13 && h->state[38 + i]; ++i)
    h->bounce[10 + i] = h->state[38 + i];
  status = h->copy(h->context, kind, ptr + split, 24, 1);
  if (status) {
    sp(h->state + 2, 0);
    return SEARCH_TERMINAL;
  }
  sp(h->state + 2, 1);
  sl(r + 24, generation);
  return 0;
}
