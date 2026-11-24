/* Thin transaction adapter over heap.c, using callbacks instead of a real GDT. */
#include "descr.h"
static void initial(struct heap_descriptor_result *r) {
    r->error=r->cleanup_error=r->reference=r->selector=0;
}
static heap_u16 invalidate_all(struct heap_state *s,heap_u16 reference,
                               const struct heap_descriptor_ops *ops) {
    heap_u16 i,count,error,first=0;
    struct heap_alias alias;
    count=(heap_u16)(s->records[reference-1u].slots*2u);
    for(i=0;i!=count;++i) {
        error=heap_alias_at(s,reference,i,&alias);
        if (!error) error=ops->invalidate(ops->context,alias.selector);
        if (error && !first) first=error;
    }
    return first;
}
void heap_descriptor_abort(struct heap_state *s,
    heap_u16 reference,const struct heap_descriptor_ops *ops,
    struct heap_descriptor_result *out) {
    struct heap_descriptor_result r;
    initial(&r);
    if (!ops || !ops->invalidate) { r.error=HEAP_BAD_ARGUMENT;*out=r;return; }
    if (!reference || reference>HEAP_RECORDS || s->records[reference-1u].phase!=1u) {
        r.error=HEAP_BAD_ORIGIN;*out=r;return;
    }
    r.reference=reference;
    r.cleanup_error=invalidate_all(s,reference,ops);
    if (!r.cleanup_error) {
        r.error=heap_abort(s,reference);
        if (!r.error) r.reference=0;
    }
    *out=r;return;
}
void heap_descriptor_allocate(struct heap_state *s,
    heap_u16 paragraphs,const struct heap_descriptor_ops *ops,heap_u16 *maximum,
    struct heap_descriptor_result *out) {
    struct heap_descriptor_result r,rollback;
    struct heap_alias alias;
    heap_u16 i,count,error;
    initial(&r);
    if (!ops || !ops->install || !ops->invalidate) {
        *maximum=heap_maximum(s);
        r.error=HEAP_BAD_ARGUMENT;
        *out=r;
        return;
    }
    r.error=heap_begin(s,paragraphs,&r.reference,maximum);
    if (r.error) { *out=r;return; }
    count=(heap_u16)(s->records[r.reference-1u].slots*2u);
    for(i=0;i!=count;++i) {
        error=heap_alias_at(s,r.reference,i,&alias);
        if (!error) error=ops->install(ops->context,&alias);
        if (error) {
            r.error=error;
            /* Include failed ordinal and unattempted/tail aliases: a failed
               callback can already have published a descriptor. */
            heap_descriptor_abort(s,r.reference,ops,&rollback);
            r.cleanup_error=rollback.cleanup_error ? rollback.cleanup_error : rollback.error;
            r.reference=rollback.reference;*out=r;return;
        }
    }
    r.error=heap_commit(s,r.reference,&r.selector);
    if (r.error) {
        heap_descriptor_abort(s,r.reference,ops,&rollback);
        r.cleanup_error=rollback.cleanup_error ? rollback.cleanup_error : rollback.error;
        r.reference=rollback.reference;
    }
    *out=r;return;
}
void heap_descriptor_release(struct heap_state *s,
    heap_u16 selector,const struct heap_descriptor_ops *ops,
    struct heap_descriptor_result *out) {
    struct heap_descriptor_result r;
    initial(&r);
    if (!ops || !ops->invalidate) { r.error=HEAP_BAD_ARGUMENT;*out=r;return; }
    r.error=heap_find_live(s,selector,&r.reference);
    if (r.error) { *out=r;return; }
    r.selector=selector;
    r.error=invalidate_all(s,r.reference,ops);
    if (!r.error) {
        r.error=heap_release(s,selector);
        if (!r.error) r.reference=r.selector=0;
    }
    /* Failed invalidation retains LIVE ownership and its exact origin. */
    *out=r;return;
}
