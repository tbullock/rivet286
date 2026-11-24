/* Same-CS callback table; request construction belongs to the extension. */
#include "pathnat.h"
static pm_word path_kernel_request(void)
{
    struct path_main_api operations;
    operations.validate=validate_buffer;operations.transfer=transfer;operations.dos=machine_dos;
    return ((pm_word (*)(pm_byte *,const struct path_main_api *))0xb000)(PM_RECORD,&operations);
}
