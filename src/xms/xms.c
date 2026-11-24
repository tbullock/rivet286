/* Isolated lifetime policy; no interrupts, far calls, GDT stores or globals. */
#include "xms.h"
static void call(xms_u16 function,xms_u16 argument,struct xms_regs *r) {
    r->ax=r->bx=r->dx=0;xms_raw(function,argument,r);
}
static void remember(struct xms_failure *f,xms_u16 operation,xms_u16 kind,
                     const struct xms_regs *r) {
    if (!f->kind) { f->operation=operation;f->kind=kind;f->raw=*r; }
}
static xms_u16 fail(struct xms_state *s,xms_u16 operation,xms_u16 kind,
                    const struct xms_regs *r) {
    s->ready=0;remember(&s->primary,operation,kind,r);return kind;
}
static xms_u16 boolean(const struct xms_regs *r) {
    if (r->ax==1) return XMS_OK;
    if (!r->ax && (r->bx&255u)>=128u) return XMS_DRIVER;
    return XMS_REPLY;
}
static int bcd_version(xms_u16 version) {
    xms_u16 i;
    for(i=0;i!=4;++i) { if ((version&15u)>9u) return 0;version=(xms_u16)(version>>4); }
    return 1;
}
void xms_initialize(struct xms_state *s) {
    struct xms_regs zero;
    zero.ax=zero.bx=zero.dx=0;
    s->started=s->ready=s->version=s->revision=s->largest_kib=s->total_kib=0;
    s->handle=s->block=s->lock=s->local=s->initial_a20=0;s->physical_base=0;
    s->primary.operation=s->primary.kind=s->cleanup.operation=s->cleanup.kind=0;
    s->primary.raw=s->cleanup.raw=zero;
}
static xms_u16 a20_query(struct xms_state *s,int require_on) {
    struct xms_regs r;
    xms_u16 kind;
    call(7,0,&r);
    if ((r.bx&255u) || r.ax>1u) {
        kind=(r.bx&255u)>=128u ? XMS_DRIVER : XMS_REPLY;
        return fail(s,7,kind,&r);
    }
    if (require_on && !r.ax) return fail(s,7,XMS_A20_OFF,&r);
    if (!require_on) s->initial_a20=r.ax;
    return XMS_OK;
}
static int a20_known_noop(const struct xms_regs *r) {
    xms_u16 error=(xms_u16)(r->bx&255u);
    return !r->ax && (error==0x80u || error==0x81u);
}
static xms_u16 acquire_local(struct xms_state *s) {
    struct xms_regs r;
    xms_u16 kind;
    call(5,0,&r);kind=boolean(&r);
    if (kind) {
        if (!a20_known_noop(&r)) {
            s->local=XMS_UNKNOWN;
            if (kind==XMS_DRIVER) kind=XMS_UNCERTAIN;
        }
        return fail(s,5,kind,&r);
    }
    s->local=XMS_HELD;return a20_query(s,1);
}
static xms_u16 cancel_local(struct xms_state *s,struct xms_regs *r) {
    xms_u16 kind;
    call(6,0,r);kind=boolean(r);
    if (!kind) s->local=XMS_NONE;
    else if (!a20_known_noop(r)) {
        s->local=XMS_UNKNOWN;
        if (kind!=XMS_REPLY) kind=XMS_UNCERTAIN;
    }
    return kind;
}
xms_u16 xms_prepare(struct xms_state *s) {
    struct xms_regs r;
    xms_u16 kind;
    r.ax=r.bx=r.dx=0;
    if (s->started) return fail(s,0xffffu,XMS_STATE,&r);
    s->started=1;
    call(0xffffu,0,&r);
    if (r.ax!=0x80u) return fail(s,0xffffu,XMS_DISCOVERY,&r);
    call(0,0,&r);s->version=r.ax;s->revision=r.bx;
    if (r.ax<0x200u || !bcd_version(r.ax)) return fail(s,0,XMS_VERSION,&r);
    call(8,0,&r);s->largest_kib=r.ax;s->total_kib=r.dx;
    if (r.bx&255u) return fail(s,8,(r.bx&255u)>=128u ? XMS_DRIVER : XMS_REPLY,&r);
    if (r.ax>r.dx) return fail(s,8,XMS_REPLY,&r);
    if (r.ax<512u) return fail(s,8,XMS_CAPACITY,&r);
    call(9,512,&r);kind=boolean(&r);
    if (kind) {
        if (kind==XMS_REPLY) s->block=XMS_UNKNOWN;
        return fail(s,9,kind,&r);
    }
    if (!r.dx) { s->block=XMS_UNKNOWN;return fail(s,9,XMS_REPLY,&r); }
    s->handle=r.dx;s->block=XMS_HELD;
    call(12,s->handle,&r);kind=boolean(&r);
    if (kind) {
        if (kind==XMS_REPLY) s->lock=XMS_UNKNOWN;
        return fail(s,12,kind,&r);
    }
    s->lock=XMS_HELD;s->physical_base=((xms_u32)r.dx<<16)|r.bx;
    if (s->physical_base<0x100000UL || s->physical_base>0xf80000UL || (s->physical_base&15u))
        return fail(s,12,XMS_PHYSICAL,&r);
    kind=a20_query(s,0);if (kind) return kind;
    kind=acquire_local(s);if (kind) return kind;
    s->ready=1;return XMS_OK;
}
xms_u16 xms_reenter(struct xms_state *s) {
    struct xms_regs r;
    xms_u16 kind;
    r.ax=r.bx=r.dx=0;
    if (!s->ready || s->primary.kind || s->block!=XMS_HELD || s->lock!=XMS_HELD || s->local!=XMS_HELD)
        return fail(s,6,XMS_STATE,&r);
    s->ready=0;kind=cancel_local(s,&r);
    if (kind) return fail(s,6,kind,&r);
    kind=acquire_local(s);if (kind) return kind;
    s->ready=1;return XMS_OK;
}
void xms_caller_error(struct xms_state *s,xms_u16 operation,xms_u16 error) {
    struct xms_regs r;
    r.ax=error;r.bx=r.dx=0;(void)fail(s,operation,XMS_CALLER,&r);
}
static void cleanup_failure(struct xms_state *s,xms_u16 operation,xms_u16 kind,
                             const struct xms_regs *r,xms_u16 *first) {
    if (!*first) *first=kind;
    remember(&s->cleanup,operation,kind,r);
}
xms_u16 xms_cleanup(struct xms_state *s) {
    struct xms_regs r;
    xms_u16 kind,first=0;
    s->ready=0;r.ax=r.bx=r.dx=0;
    if (s->local==XMS_UNKNOWN) {
        r.ax=r.bx=r.dx=0;cleanup_failure(s,6,XMS_UNCERTAIN,&r,&first);
    }
    else if (s->local==XMS_HELD) {
        kind=cancel_local(s,&r);
        if (kind) cleanup_failure(s,6,kind,&r,&first);
    }
    /* The failed client cannot use the arena; unrelated A20 uncertainty does
       not justify leaking an otherwise known lock/block. */
    if (s->lock==XMS_UNKNOWN) {
        r.ax=r.bx=r.dx=0;cleanup_failure(s,13,XMS_UNCERTAIN,&r,&first);
    }
    else if (s->lock==XMS_HELD) {
        call(13,s->handle,&r);kind=boolean(&r);
        if (!kind) s->lock=XMS_NONE;
        else {
            if (kind==XMS_REPLY) s->lock=XMS_UNKNOWN;
            cleanup_failure(s,13,kind,&r,&first);
        }
    }
    if (s->block==XMS_UNKNOWN) {
        r.ax=r.bx=r.dx=0;cleanup_failure(s,10,XMS_UNCERTAIN,&r,&first);
    }
    else if (s->block==XMS_HELD && s->lock==XMS_NONE) {
        call(10,s->handle,&r);kind=boolean(&r);
        if (!kind) { s->block=XMS_NONE;s->handle=0; }
        else {
            if (kind==XMS_REPLY) s->block=XMS_UNKNOWN;
            cleanup_failure(s,10,kind,&r,&first);
        }
    }
    return first;
}
