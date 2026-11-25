#ifndef PM_SEARCH_POLICY_H
#define PM_SEARCH_POLICY_H
#include "request.h"
#define SEARCH_FIRST 11u
#define SEARCH_NEXT 12u
#define SEARCH_TERMINAL 11u
#define SEARCH_FAILSTOP 12u /* Gateway containment: never ordinary cleanup. */
#define SEARCH_FAILSTOP_INSTALL                                                \
  13u /* Install mismatch and restoration failed. */
#define SEARCH_STATE_BYTES 64u
struct search_pointer {
  pm_word offset, segment;
};
struct search_dos_result {
  pm_word ax, flags;
};
struct search_host {
  void *context;
  pm_byte *state;  /* Explicit64-byte9FC0 state, not host struct layout. */
  pm_byte *bounce; /* Existing512-byte6200 bounce. */
  pm_word profile, active, ready,
      binding; /* Trusted9F00/04,95C4,9F02 values. */
  pm_word (*access)(void *, pm_word, pm_dword, pm_word, pm_word);
  pm_word (*copy)(void *, pm_word, pm_dword, pm_word, pm_word);
  pm_word (*transaction)(const struct search_host *, pm_word, pm_word,
                         struct search_dos_result *);
#ifdef PMTEST_SEARCH_HOST_MODEL
  struct search_pointer (*get_dta)(void *);
  void (*set_dta)(void *, struct search_pointer);
  void (*find)(void *, pm_word, pm_word, pm_byte *, pm_byte *,
               struct search_dos_result *);
  struct search_pointer
      private_dta; /* Host model only; actual gateway uses fixed hostCS:9FC8. */
#endif
};
/* Full transaction model is portable-test-only. Target uses one gateway call.
 */
#ifdef PMTEST_SEARCH_HOST_MODEL
pm_word search_transaction_model(const struct search_host *, pm_word, pm_word,
                                 struct search_dos_result *);
#endif
#ifdef PMTEST_SEARCH_HOST_MODEL
void search_initialize(pm_byte *);
void search_invalidate(pm_byte *);
void search_launch(pm_byte *, pm_word);
#endif
pm_word search_dispatch(pm_byte *, const struct search_host *);
#endif
