/* Same-CS adapter lives in the extension, outside the scarce main slot. */
#include "pathnat.h"
#include "execute.h"
#include "search.h"
#ifndef PMTEST_PATH_HOST
#define PATH_AT(offset) ((pm_byte *)(offset))
#else
pm_byte *path_test_address(pm_word offset);
#define PATH_AT(offset) path_test_address(offset)
#endif
static pm_word path_access(void *context, pm_word kind, pm_dword pointer,
                           pm_word count, pm_word writing) {
  const struct path_main_api *api = (const struct path_main_api *)context;
  struct pm_request request;
  request.operation = writing ? PM_REQUEST_READ : PM_REQUEST_WRITE;
  request.kind = kind;
  request.pointer = pointer;
  request.count = count;
  return api->validate(&request);
}
static pm_word path_copy(void *context, pm_word kind, pm_dword pointer,
                         pm_word count, pm_word writing) {
  const struct path_main_api *api = (const struct path_main_api *)context;
  struct pm_request request;
  request.kind = kind;
  request.pointer = pointer;
  return api->transfer(&request, 0, count, writing);
}
static pm_word path_dos(void *context, pm_word ax, pm_word bx, pm_word cx,
                        pm_word dx, struct path_dos_result *returned) {
  const struct path_main_api *api = (const struct path_main_api *)context;
  pm_byte *packet = PATH_AT(0x6100);
  pm_word status;
  put(packet + 2, ax);
  put(packet + 4, bx);
  put(packet + 6, cx);
  put(packet + 8, dx);
  put(packet + 10, 0);
  status = api->dos();
  if (status)
    return status;
  returned->ax = word(packet + 2);
  returned->cx = word(packet + 6);
  returned->flags = word(packet + 10);
  return 0;
}
/* One protected call owns the entire real save/install/find/restore interval.
 */
static pm_word path_search_transaction(const struct search_host *h, pm_word op,
                                       pm_word attrs,
                                       struct search_dos_result *returned) {
  struct path_dos_result raw;
  pm_word status;
  status = path_dos(h->context, (pm_word)(op == SEARCH_FIRST ? 0x4e00 : 0x4f00),
                    0, attrs, (pm_word)(op == SEARCH_FIRST ? 0x6200 : 0), &raw);
  if (status)
    return status;
  /* Search-only packetCX is transport0/6, never a fabricated DOS result. */
  if (raw.cx)
    return (pm_word)(raw.cx == 6u ? 6u : SEARCH_TERMINAL);
  returned->ax = raw.ax;
  returned->flags = raw.flags;
  return 0;
}
static pm_word path_execute_admitted(void *context, pm_word binding) {
  (void)context;
  return (pm_word)(word(PATH_AT(0x9f00)) == 1 && word(PATH_AT(0x9f04)) == 1 &&
                   word(PATH_AT(0x95c4)) == 1 && binding &&
                   binding == word(PATH_AT(0x9f02)));
}
static pm_word path_execute_access(void *context, pm_word kind,
                                   pm_dword pointer, pm_word count) {
  return path_access(context, kind, pointer, count, 0);
}
static pm_word path_execute_copy(void *context, pm_word kind, pm_dword pointer,
                                 pm_word count) {
  return path_copy(context, kind, pointer, count, 0);
}
static pm_word path_execute_run(void *context, pm_word split, pm_word tail,
                                struct execute_result *returned) {
  struct path_dos_result raw;
  pm_word status;
  status = path_dos(context, 0xff4b, split, tail, 0x6200, &raw);
  if (status)
    return status;
  if (raw.cx)
    return EXECUTE_TERMINAL;
  if (raw.flags & 1u) {
    returned->launched = 0;
    returned->error = raw.ax;
    returned->child = 0;
  } else {
    returned->launched = 1;
    returned->error = 0;
    returned->child = raw.ax;
  }
  return 0;
}
pm_word path_extension(pm_byte *record, const struct path_main_api *api) {
  struct path_host operations;
  struct search_host search;
  struct execute_host execute;
  if (word(PATH_AT(0x611e)) == 0x0600) {
    execute.context = (void *)api;
    execute.bounce = PATH_AT(0x6200);
    execute.admitted = path_execute_admitted;
    execute.access = path_execute_access;
    execute.copy = path_execute_copy;
    execute.execute = path_execute_run;
    return execute_dispatch(record, &execute);
  }
  if (word(record) == 2u && word(record + 2) >= SEARCH_FIRST) {
    search.context = (void *)api;
    search.bounce = PATH_AT(0x6200);
    search.state = PATH_AT(0x9fc0);
    search.profile = word(PATH_AT(0x9f00));
    search.binding = word(PATH_AT(0x9f02));
    search.active = word(PATH_AT(0x9f04));
    search.ready = word(PATH_AT(0x95c4));
    search.access = path_access;
    search.copy = path_copy;
    search.transaction = path_search_transaction;
    return search_dispatch(record, &search);
  }
  operations.context = (void *)api;
  operations.bounce = PATH_AT(0x6200);
  operations.handles = (pm_word *)PATH_AT(0x6040);
  operations.access = path_access;
  operations.copy = path_copy;
  operations.dos = path_dos;
  return path_dispatch(record, &operations);
}
