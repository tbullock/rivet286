#ifndef PMTEST_REQUEST_H
#define PMTEST_REQUEST_H
#ifdef _MSC_VER
typedef unsigned long pm_dword;
#else
#include <stdint.h>
typedef uint32_t pm_dword;
#endif
typedef unsigned char pm_byte;
typedef unsigned short pm_word;
typedef char pm_word_width[(sizeof(pm_word) == 2) ? 1 : -1];
typedef char pm_dword_width[(sizeof(pm_dword) == 4) ? 1 : -1];
#define PM_REQUEST_VERSION 1u
#define PM_REQUEST_VERSION_EXTENDED 2u
#define PM_REQUEST_BYTES 32u
#define PM_REQUEST_QUERY 0u
#define PM_REQUEST_TIME 1u
#define PM_REQUEST_OPEN 2u
#define PM_REQUEST_CREATE 3u
#define PM_REQUEST_READ 4u
#define PM_REQUEST_WRITE 5u
#define PM_REQUEST_SEEK 6u
#define PM_REQUEST_CLOSE 7u
#define PM_REQUEST_BORROW_READ 8u
#define PM_REQUEST_BORROW_WRITE 9u
#define PM_REQUEST_TRUNCATE 10u
#define PM_REQUEST_BORROW_SEEK 11u
#define PM_REQUEST_BORROW_TRUNCATE 12u
#define PM_REQUEST_OK 0u
#define PM_REQUEST_BAD_LENGTH 1u
#define PM_REQUEST_BAD_VERSION 2u
#define PM_REQUEST_BAD_OPERATION 3u
#define PM_REQUEST_BAD_RESERVED 4u
#define PM_REQUEST_BAD_EXTENT 5u
#define PM_REQUEST_BUSY 6u
#define PM_REQUEST_BAD_HANDLE 7u
#define PM_REQUEST_BAD_ARGUMENT 8u
#define PM_REQUEST_DOS_FAILURE 9u
#define PM_REQUEST_HANDLE_FULL 10u
#define PM_BUFFER_NONE 0u
#define PM_BUFFER_SELECTOR 1u
#define PM_BUFFER_LOGICAL 2u
struct pm_request {
    pm_word version, operation, flags, handle, kind, origin;
    pm_dword pointer, count, displacement;
};
pm_word pm_read_word(const pm_byte *bytes);
pm_dword pm_read_dword(const pm_byte *bytes);
void pm_write_word(pm_byte *bytes, pm_word value);
void pm_write_dword(pm_byte *bytes, pm_dword value);
pm_word pm_request_decode(const pm_byte *bytes, pm_word length,
                          struct pm_request *result);
pm_word pm_request_extent(pm_word offset, pm_word length, pm_word limit);
#endif
