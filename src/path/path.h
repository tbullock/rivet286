#ifndef PM_PATH_OWNER_H
#define PM_PATH_OWNER_H
#include "request.h"
/* Separate AX0500 namespace; AX0100 v1/v2 retain their contract. */
#define PATH_VERSION 1u
#define PATH_TERMINAL 11u /* Private: outer owner must stop, never IRET. */
#define PATH_QUERY 0u
#define PATH_DRIVE 1u
#define PATH_CWD 2u
#define PATH_GETATTR 3u
#define PATH_SETATTR 4u
#define PATH_DELETE 5u
#define PATH_MKDIR 6u
#define PATH_RMDIR 7u
#define PATH_CHDIR 8u
#define PATH_RENAME 9u
#define PATH_EXCLUSIVE 10u
struct path_dos_result { pm_word ax, cx, flags; };
struct path_host {
    void *context;
    pm_byte *bounce; /* The sole existing512-byte host bounce at6200. */
    pm_word *handles; /* The sole existing6040 table; FFFF means vacant. */
    /* Main near owner reuses validate_buffer, including heap ownership.
       writing1 means client output, writing0 means client input. */
    pm_word (*access)(void *,pm_word,pm_dword,pm_word,pm_word);
    /* Main owner reuses transfer: whole input/output at existing6200 only. */
    pm_word (*copy)(void *,pm_word,pm_dword,pm_word,pm_word);
    /* Gateway validates fixed DOS pointers; no protected selector reaches DOS.
       BX is rename split, CX total staged bytes; other ops use native regs. */
    pm_word (*dos)(void *,pm_word,pm_word,pm_word,pm_word,struct path_dos_result *);
};
pm_word path_dispatch(pm_byte *,const struct path_host *);
#endif
