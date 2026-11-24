/* Include in the near kernel translation unit: no target static storage. */
#define PM_AT(offset) ((pm_byte *)(offset))
#define PM_RECORD PM_AT(0x6000)
#define PM_HANDLES PM_AT(0x6040)
#define PM_PACKET PM_AT(0x6100)
#define PM_TRANSFER PM_AT(0x6200)
pm_word machine_dos(void);
pm_word machine_copy(pm_word selector, pm_word offset, pm_word host_offset,
                     pm_word count, pm_word direction);

/* Defined after HEAPALL.C; keep private heap ownership out of the wire ABI. */
static pm_word request_heap_extent(pm_dword address, pm_dword count);
static void request_init(void)
{
    pm_word i;
    for (i = 0; i != 8; ++i) pm_write_word(PM_HANDLES + i * 2, 0xffff);
}
static void packet(pm_word ax, pm_word bx, pm_word cx, pm_word dx)
{
    pm_write_word(PM_PACKET + 2, ax); pm_write_word(PM_PACKET + 4, bx);
    pm_write_word(PM_PACKET + 6, cx); pm_write_word(PM_PACKET + 8, dx);
    pm_write_word(PM_PACKET + 10, 0);
}
static pm_word invoke(void)
{
    pm_word status = machine_dos();
    if (status) return status;
    if (pm_read_word(PM_PACKET + 10) & 1) {
        pm_write_word(PM_RECORD + 28, pm_read_word(PM_PACKET + 2));
        return PM_REQUEST_DOS_FAILURE;
    }
    return 0;
}
/* Directions2/3 validate readable/writable extents without copying. */
static pm_word transfer(const struct pm_request *r, pm_dword done,
                        pm_word count, pm_word direction)
{
    pm_dword p = r->pointer;
    pm_word selector, offset;
    if (r->kind == PM_BUFFER_LOGICAL) {
        /* Loader model5 is the only admitted logical32 mapping. */
        if (pm_read_word(PM_AT(0x102e)) != 0x50) return PM_REQUEST_BAD_EXTENT;
        p += done;
        if (p < 0x10000UL || p >= 0x10000000UL)
            return PM_REQUEST_BAD_EXTENT;
        selector = (pm_word)((p >> 15) << 3);
        offset = (pm_word)(p & 0x7fffUL);
    } else {
        selector = (pm_word)(p >> 16);
        if ((pm_dword)(pm_word)p + done + count > 65536UL)
            return PM_REQUEST_BAD_EXTENT;
        offset = (pm_word)((pm_word)p + done);
    }
    return machine_copy(selector, offset, 0x6200, count, direction);
}
static pm_word validate_buffer(const struct pm_request *r)
{
    pm_dword done = 0;
    pm_word count, status;
    pm_dword arena_end;
    if (r->count) {
        if (r->kind == PM_BUFFER_SELECTOR) {
            pm_word selector = (pm_word)(r->pointer >> 16);
            if (selector >= 0x100) {
                if (selector > 0x7f8 || (selector & 7) ||
                    r->count > 65536UL - (pm_dword)(pm_word)r->pointer)
                    return PM_REQUEST_BAD_EXTENT;
                status = request_heap_extent(((pm_dword)selector << 12) +
                    (pm_dword)(pm_word)r->pointer, r->count);
                if (status) return status;
            }
        } else if (r->kind == PM_BUFFER_LOGICAL && r->pointer >= 0x100000UL) {
            if (pm_read_word(PM_AT(0x102e)) != 0x50 ||
                request_heap_extent(r->pointer, r->count))
                return PM_REQUEST_BAD_EXTENT;
        }
    }
    if (r->count && r->kind == PM_BUFFER_LOGICAL
        && r->pointer < 0x100000UL
       ) {
        if (r->pointer >= 0x40000UL && r->pointer < 0x50000UL)
            arena_end = 0x50000UL;
        else if (r->pointer >= 0x50000UL && r->pointer < 0x60000UL)
            arena_end = 0x60000UL;
        else if (r->pointer >= 0x70000UL && r->pointer < 0xa0000UL)
            arena_end = 0xa0000UL;
        else return PM_REQUEST_BAD_EXTENT;
        if (r->count > arena_end - r->pointer) return PM_REQUEST_BAD_EXTENT;
    }
    while (done < r->count) {
        count = r->count - done > 512UL ? 512 : (pm_word)(r->count - done);
        status = transfer(r, done, count,
            (pm_word)((r->operation == PM_REQUEST_READ ||
                       r->operation == PM_REQUEST_BORROW_READ ||
                       r->operation == PM_REQUEST_TIME) ? 3 : 2));
        if (status) return status;
        done += count;
    }
    return 0;
}
pm_word kernel_request(void)
{
    struct pm_request r;
    pm_word status, slot, handle, count, amount, i;
    pm_dword done = 0;
    /* Channel is captured from original AX by the trusted INT60 bridge.
       Only the NUMERIC monitor admits0900; ordinary profiles reject it.
       Query snapshots are integer only: no probe or second context exchange. */
    status = pm_request_decode(PM_RECORD, PM_REQUEST_BYTES, &r);
    if (status) return status;
    if (r.operation == PM_REQUEST_QUERY) {
        pm_write_dword(PM_RECORD + 24, r.version == PM_REQUEST_VERSION ?
            0xffUL : 0x1fffUL); return 0;
    }
    status = validate_buffer(&r);
    if (status) return status;
    if (r.operation == PM_REQUEST_TIME) {
        packet(0x2a00, 0, 0, 0);
        status = machine_dos();
        if (status) return status;
        pm_write_word(PM_TRANSFER, pm_read_word(PM_PACKET + 6));
        PM_TRANSFER[2] = PM_PACKET[9]; PM_TRANSFER[3] = PM_PACKET[8];
        packet(0x2c00, 0, 0, 0); status = machine_dos(); if (status) return status;
        PM_TRANSFER[4] = PM_PACKET[7]; PM_TRANSFER[5] = PM_PACKET[6];
        PM_TRANSFER[6] = PM_PACKET[9]; PM_TRANSFER[7] = PM_PACKET[8];
        status = transfer(&r, 0, 8, 1);
        if (!status) pm_write_dword(PM_RECORD + 24, 8UL);
        return status;
    }
    if (r.operation == PM_REQUEST_OPEN || r.operation == PM_REQUEST_CREATE) {
        for (slot = 0; slot != 8; ++slot)
            if (pm_read_word(PM_HANDLES + slot * 2) == 0xffff) break;
        if (slot == 8) return PM_REQUEST_HANDLE_FULL;
        status = transfer(&r, 0, (pm_word)r.count, 0); if (status) return status;
        /* Reject embedded NUL and missing terminal NUL before DOS sees a path. */
        for (i = 0; (pm_dword)i + 1UL < r.count; ++i)
            if (PM_TRANSFER[i] == 0) return PM_REQUEST_BAD_ARGUMENT;
        if (PM_TRANSFER[r.count - 1] != 0) return PM_REQUEST_BAD_ARGUMENT;
        packet(r.operation == PM_REQUEST_OPEN ? (pm_word)(0x3d00 | r.flags) :
               0x3c00, 0, 0, 0x6200);
        status = invoke(); if (status) return status;
        pm_write_word(PM_HANDLES + slot * 2, pm_read_word(PM_PACKET + 2));
        pm_write_dword(PM_RECORD + 24, (pm_dword)slot + 1UL); return 0;
    }
    if (r.operation == PM_REQUEST_BORROW_READ ||
        r.operation == PM_REQUEST_BORROW_WRITE || r.operation == PM_REQUEST_BORROW_SEEK ||
        r.operation == PM_REQUEST_BORROW_TRUNCATE) {
        /* Raw standard DOS handles are borrowed, never table slots or closed. */
        if (r.handle > 4) return PM_REQUEST_BAD_HANDLE;
        /* DOS open may return a low raw handle when a standard was closed.
           Ownership then takes precedence over borrowing the same raw value. */
        for (i = 0; i != 8; ++i)
            if (pm_read_word(PM_HANDLES + i * 2) == r.handle)
                return PM_REQUEST_BAD_HANDLE;
        handle = r.handle;
        slot = 0; /* Not used by a borrowed operation. */
        if (r.operation == PM_REQUEST_BORROW_READ) r.operation = PM_REQUEST_READ;
        else if (r.operation == PM_REQUEST_BORROW_WRITE) r.operation = PM_REQUEST_WRITE;
        else if (r.operation == PM_REQUEST_BORROW_SEEK) r.operation = PM_REQUEST_SEEK;
        else r.operation = PM_REQUEST_TRUNCATE;
    } else {
        if (r.handle < 1 || r.handle > 8) return PM_REQUEST_BAD_HANDLE;
        slot = (pm_word)(r.handle - 1);
        handle = pm_read_word(PM_HANDLES + slot * 2);
        if (handle == 0xffff) return PM_REQUEST_BAD_HANDLE;
    }
    if (r.operation == PM_REQUEST_TRUNCATE) {
        /* Private gateway tag: admitted only with CX0/DX6200, then normalized
           to AH40 before injection or DOS. Ordinary zero writes stay forbidden. */
        packet(0xff40, handle, 0, 0x6200);
        status = invoke();
        if (status) return status;
        return pm_read_word(PM_PACKET + 2) == 0 ? 0 : PM_REQUEST_BAD_ARGUMENT;
    }
    if (r.operation == PM_REQUEST_CLOSE) {
        packet(0x3e00, handle, 0, 0); status = invoke();
        if (!status) pm_write_word(PM_HANDLES + slot * 2, 0xffff);
        return status;
    }
    if (r.operation == PM_REQUEST_SEEK) {
        packet((pm_word)(0x4200 | r.origin), handle,
               (pm_word)(r.displacement >> 16), (pm_word)r.displacement);
        status = invoke();
        if (!status) pm_write_dword(PM_RECORD + 24,
            (pm_dword)pm_read_word(PM_PACKET + 2) |
            ((pm_dword)pm_read_word(PM_PACKET + 8) << 16));
        return status;
    }
    /* Count zero is a NOOP: DOS AH40 count0 would truncate or extend the file. */
    while (done < r.count) {
        count = r.count - done > 512UL ? 512 : (pm_word)(r.count - done);
        if (r.operation == PM_REQUEST_WRITE) {
            status = transfer(&r, done, count, 0); if (status) return status;
        }
        packet(r.operation == PM_REQUEST_READ ? 0x3f00 : 0x4000,
               handle, count, 0x6200);
        status = invoke(); if (status) return status;
        amount = pm_read_word(PM_PACKET + 2);
        if (amount > count) return PM_REQUEST_BAD_ARGUMENT;
        if (r.operation == PM_REQUEST_READ && amount) {
            status = transfer(&r, done, amount, 1); if (status) return status;
        }
        done += amount; pm_write_dword(PM_RECORD + 24, done);
        if (amount < count) break;
    }
    return 0;
}

/* Close every remaining owned handle, including after a failing client. */
static pm_word request_cleanup(void)
{
    pm_word slot, handle, status, first_error = 0;
    for (slot = 0; slot != 8; ++slot) {
        handle = pm_read_word(PM_HANDLES + slot * 2);
        if (handle == 0xffff) continue;
        packet(0x3e00, handle, 0, 0);
        status = invoke();
        if (!status) pm_write_word(PM_HANDLES + slot * 2, 0xffff);
        else if (!first_error) first_error = status;
    }
    return first_error;
}
