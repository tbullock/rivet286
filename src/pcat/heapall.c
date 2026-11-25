/* The heap and hosted services share the kernel's near translation unit. */
/* Include the implementation files in declaration order. */
/* clang-format off */
#include "heap.c"
#include "ranges.c"
#include "descr.c"
#include "segments.c"
#include "commands.c"
#include "xms.c"
#include "heapkern.c"
#include "heapgdt.h"
#include "pathkern.c"
#include "startup.c"
#include "startnat.c"
#include "startmon.c"
/* clang-format on */
