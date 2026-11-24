#ifndef PM_PATH_MAIN_API_H
#define PM_PATH_MAIN_API_H
#include "request.h"
/* Exact existing near C6 signatures. Never public wire/function pointers. */
struct path_main_api {
    pm_word (*validate)(const struct pm_request *);
    pm_word (*transfer)(const struct pm_request *,pm_dword,pm_word,pm_word);
    pm_word (*dos)(void);
};
pm_word path_extension(pm_byte *,const struct path_main_api *);
#endif
