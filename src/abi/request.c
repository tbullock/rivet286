#include "request.h"
pm_word pm_read_word(const pm_byte *p)
{
    return (pm_word)((pm_word)p[0] | ((pm_word)p[1] << 8));
}
pm_dword pm_read_dword(const pm_byte *p)
{
    return (pm_dword)pm_read_word(p) | ((pm_dword)pm_read_word(p + 2) << 16);
}
void pm_write_word(pm_byte *p, pm_word value)
{
    p[0] = (pm_byte)value; p[1] = (pm_byte)(value >> 8);
}
void pm_write_dword(pm_byte *p, pm_dword value)
{
    pm_write_word(p, (pm_word)value);
    pm_write_word(p + 2, (pm_word)(value >> 16));
}
pm_word pm_request_extent(pm_word offset, pm_word length, pm_word limit)
{
    pm_dword end;
    if (length == 0)
        return PM_REQUEST_BAD_EXTENT;
    end = (pm_dword)offset + length;
    return end <= (pm_dword)limit + 1UL ? 0 : PM_REQUEST_BAD_EXTENT;
}
pm_word pm_request_decode(const pm_byte *bytes, pm_word length,
                          struct pm_request *result)
{
    struct pm_request r;
    pm_word read_write, seek, truncate;
    if (length != PM_REQUEST_BYTES)
        return PM_REQUEST_BAD_LENGTH;
    r.version = pm_read_word(bytes);
    if (r.version != PM_REQUEST_VERSION && r.version != PM_REQUEST_VERSION_EXTENDED)
        return PM_REQUEST_BAD_VERSION;
    if (pm_read_word(bytes + 30) || pm_read_dword(bytes + 24) ||
        pm_read_word(bytes + 28))
        return PM_REQUEST_BAD_RESERVED;
    r.operation = pm_read_word(bytes + 2);
    if (r.operation > (r.version == PM_REQUEST_VERSION ? PM_REQUEST_CLOSE :
                       PM_REQUEST_BORROW_TRUNCATE))
        return PM_REQUEST_BAD_OPERATION;
    read_write = (pm_word)(r.operation == PM_REQUEST_READ ||
        r.operation == PM_REQUEST_WRITE || r.operation == PM_REQUEST_BORROW_READ ||
        r.operation == PM_REQUEST_BORROW_WRITE);
    seek = (pm_word)(r.operation == PM_REQUEST_SEEK || r.operation == PM_REQUEST_BORROW_SEEK);
    truncate = (pm_word)(r.operation == PM_REQUEST_TRUNCATE ||
        r.operation == PM_REQUEST_BORROW_TRUNCATE);
    r.flags = pm_read_word(bytes + 4);
    r.handle = pm_read_word(bytes + 6);
    r.kind = pm_read_word(bytes + 8);
    r.origin = pm_read_word(bytes + 10);
    r.pointer = pm_read_dword(bytes + 12);
    r.count = pm_read_dword(bytes + 16);
    r.displacement = pm_read_dword(bytes + 20);
    if (r.kind > PM_BUFFER_LOGICAL)
        return PM_REQUEST_BAD_ARGUMENT;
    if (r.operation == PM_REQUEST_OPEN) {
        if (r.flags > 2) return PM_REQUEST_BAD_ARGUMENT;
    } else if (r.flags) return PM_REQUEST_BAD_ARGUMENT;
    if (!seek && (r.origin || r.displacement))
        return PM_REQUEST_BAD_ARGUMENT;
    if (seek && r.origin > 2)
        return PM_REQUEST_BAD_ARGUMENT;
    if (r.operation == PM_REQUEST_QUERY || seek || truncate ||
        r.operation == PM_REQUEST_CLOSE) {
        if (r.kind || r.pointer || r.count) return PM_REQUEST_BAD_ARGUMENT;
    } else if (!r.kind || r.count == 0) {
        /* Zero-size I/O is valid and never sent to DOS. */
        if (!read_write)
            return PM_REQUEST_BAD_ARGUMENT;
        if (r.count != 0 || (r.kind == 0 && r.pointer != 0))
            return PM_REQUEST_BAD_ARGUMENT;
    }
    if (!read_write && !seek && !truncate &&
        r.operation != PM_REQUEST_CLOSE &&
        r.handle) return PM_REQUEST_BAD_ARGUMENT;
    if (r.operation == PM_REQUEST_TIME && r.count != 8)
        return PM_REQUEST_BAD_ARGUMENT;
    if ((r.operation == PM_REQUEST_OPEN || r.operation == PM_REQUEST_CREATE) &&
        (r.count < 2 || r.count > 512)) return PM_REQUEST_BAD_ARGUMENT;
    if (r.kind == PM_BUFFER_SELECTOR &&
        ((pm_dword)(pm_word)r.pointer + r.count > 65536UL))
        return PM_REQUEST_BAD_EXTENT;
    if (r.count && r.pointer > 0xffffffffUL - (r.count - 1UL))
        return PM_REQUEST_BAD_EXTENT;
    result->version = r.version; result->operation = r.operation; result->flags = r.flags;
    result->handle = r.handle; result->kind = r.kind;
    result->origin = r.origin; result->pointer = r.pointer;
    result->count = r.count; result->displacement = r.displacement;
    return PM_REQUEST_OK;
}
