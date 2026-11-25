#ifndef PM_EXECUTE_H
#define PM_EXECUTE_H
#include "request.h"
#define EXECUTE_VERSION 1u
#define EXECUTE_TERMINAL 11u
struct execute_result {
  pm_word launched; /* 0: raw launch error, 1: child returned. */
  pm_word error;
  pm_word child; /* DOS4D AH termination kind, AL child return value. */
};
struct execute_host {
  void *context;
  pm_byte *bounce; /* Existing512-byte host bounce. No client paragraphs. */
  pm_word (*admitted)(void *, pm_word);
  pm_word (*access)(void *, pm_word, pm_dword, pm_word);
  pm_word (*copy)(void *, pm_word, pm_dword, pm_word);
  /* One real interval: owned path/tail/FCBs/parameters, EXEC then4D.
     Ordinary transport errors are legal only before child execution.
     After4B, unsafe recovery must fail stopped inside the gateway. */
  pm_word (*execute)(void *, pm_word, pm_word, struct execute_result *);
};
pm_word execute_dispatch(pm_byte *, const struct execute_host *);
#endif
