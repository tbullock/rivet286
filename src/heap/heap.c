/* Isolated metadata algorithm. No XMS calls, GDT stores or runtime replacement. */
#include "heap.h"
#define PENDING 1u
#define LIVE 2u
static int occupied(const struct heap_state *s, heap_u16 paragraph) {
    return (s->physical[paragraph/8u] & (1u<<(paragraph%8u)))!=0;
}
static void mark(struct heap_state *s, heap_u16 paragraph, int used) {
    heap_u8 mask=(heap_u8)(1u<<(paragraph%8u));
    if (used) s->physical[paragraph/8u]|=mask;
    else s->physical[paragraph/8u]&=(heap_u8)~mask;
}
static void clear_record(struct heap_record *r) {
    r->phase=0; r->logical_first=0; r->slots=0;
    r->physical_first=0; r->paragraphs=0;
}
heap_u16 heap_initialize(struct heap_state *s, heap_u32 physical_base) {
    heap_u16 i;
    if (physical_base<0x100000UL || physical_base>0xf80000UL || (physical_base&15u))
        return HEAP_BAD_ARGUMENT;
    s->physical_base=physical_base;
    for (i=0;i!=HEAP_PARAGRAPHS/8u;++i) s->physical[i]=0;
    for (i=0;i!=HEAP_SLOTS;++i) s->logical[i]=0;
    for (i=0;i!=HEAP_RECORDS;++i) clear_record(&s->records[i]);
    return HEAP_OK;
}
heap_u16 heap_maximum(const struct heap_state *s) {
    heap_u16 i, bit, run, physical, logical;
    heap_u8 bits;
    heap_u32 bound;
    for (i=0;i!=HEAP_RECORDS && s->records[i].phase;++i) {}
    if (i==HEAP_RECORDS) return 0;
    run=physical=0;
    /* A clear byte extends the preceding run by eight; an occupied byte
       breaks it. Only mixed bytes need individual low-to-high bit checks.
       The full32768-paragraph run fits heap_u16, including on the 286. */
    for (i=0;i!=HEAP_PARAGRAPHS/8u;++i) {
        bits=s->physical[i];
        if (!bits) {
            run=(heap_u16)(run+8u);
            if (run>physical) physical=run;
        } else if (bits==255u) run=0;
        else {
            for (bit=0;bit!=8u;++bit) {
                if (bits&1u) run=0;
                else { ++run; if (run>physical) physical=run; }
                bits=(heap_u8)(bits>>1);
            }
        }
    }
    run=logical=0;
    for (i=0;i!=HEAP_SLOTS;++i) {
        if (s->logical[i]) run=0;
        else { ++run; if (run>logical) logical=run; }
    }
    bound=(heap_u32)logical*4096u;
    if (bound<physical) return (heap_u16)bound;
    return physical;
}
heap_u16 heap_begin(struct heap_state *s, heap_u16 paragraphs,
                    heap_u16 *ticket, heap_u16 *maximum) {
    heap_u16 index, physical, logical, slots, i, run;
    struct heap_record *r;
    *ticket=0; *maximum=heap_maximum(s);
    if (!paragraphs) return HEAP_BAD_ARGUMENT;
    if (paragraphs>*maximum) return HEAP_NOMEM;
    slots=(heap_u16)(((heap_u32)paragraphs+4095u)/4096u);
    for (index=0;index!=HEAP_RECORDS && s->records[index].phase;++index) {}
    if (index==HEAP_RECORDS) return HEAP_NOMEM;
    run=0;physical=0;
    for (i=0;i!=HEAP_PARAGRAPHS;++i) {
        if (occupied(s,i)) run=0; else ++run;
        if (run==paragraphs) { physical=(heap_u16)(i+1u-run);break; }
    }
    if (i==HEAP_PARAGRAPHS) return HEAP_NOMEM;
    run=0;logical=0;
    for (i=0;i!=HEAP_SLOTS;++i) {
        if (s->logical[i]) run=0; else ++run;
        if (run==slots) { logical=(heap_u16)(i+1u-run);break; }
    }
    if (i==HEAP_SLOTS) return HEAP_NOMEM;
    /* All fallible searches precede reservation; no live origin exists yet. */
    for (i=0;i!=paragraphs;++i) mark(s,(heap_u16)(physical+i),1);
    for (i=0;i!=slots;++i) s->logical[logical+i]=1;
    r=&s->records[index]; r->phase=PENDING;r->logical_first=logical;
    r->slots=slots;r->physical_first=physical;r->paragraphs=paragraphs;
    *ticket=(heap_u16)(index+1u);return HEAP_OK;
}
heap_u16 heap_commit(struct heap_state *s, heap_u16 ticket, heap_u16 *selector) {
    struct heap_record *r;
    *selector=0;
    if (!ticket || ticket>HEAP_RECORDS) return HEAP_BAD_ORIGIN;
    r=&s->records[ticket-1u];
    if (r->phase!=PENDING) return HEAP_BAD_ORIGIN;
    r->phase=LIVE;
    *selector=(heap_u16)(0x100u+r->logical_first*16u);
    return HEAP_OK;
}
static void unreserve(struct heap_state *s, struct heap_record *r) {
    heap_u16 i;
    for (i=0;i!=r->paragraphs;++i) mark(s,(heap_u16)(r->physical_first+i),0);
    for (i=0;i!=r->slots;++i) s->logical[r->logical_first+i]=0;
    clear_record(r);
}
heap_u16 heap_abort(struct heap_state *s, heap_u16 ticket) {
    if (!ticket || ticket>HEAP_RECORDS || s->records[ticket-1u].phase!=PENDING)
        return HEAP_BAD_ORIGIN;
    unreserve(s,&s->records[ticket-1u]);return HEAP_OK;
}
heap_u16 heap_find_live(const struct heap_state *s, heap_u16 selector,
                         heap_u16 *reference) {
    heap_u16 i, slot;
    *reference=0;
    if (selector<0x100u || selector>0x7f0u || (selector&15u)) return HEAP_BAD_ORIGIN;
    slot=(heap_u16)((selector-0x100u)/16u);
    for (i=0;i!=HEAP_RECORDS;++i) {
        if (s->records[i].phase==LIVE && s->records[i].logical_first==slot) {
            *reference=(heap_u16)(i+1u);return HEAP_OK;
        }
    }
    return HEAP_BAD_ORIGIN;
}
heap_u16 heap_release(struct heap_state *s, heap_u16 selector) {
    heap_u16 reference, error;
    error=heap_find_live(s,selector,&reference);
    if (error) return error;
    unreserve(s,&s->records[reference-1u]);return HEAP_OK;
}
heap_u16 heap_alias_at(const struct heap_state *s, heap_u16 ticket,
                       heap_u16 index, struct heap_alias *alias) {
    const struct heap_record *r;
    heap_u32 offset, remaining;
    alias->physical_base=0;alias->selector=0;alias->limit=0;alias->present=0;
    if (!ticket || ticket>HEAP_RECORDS) return HEAP_BAD_ORIGIN;
    r=&s->records[ticket-1u];
    if (!r->phase || index>=r->slots*2u) return HEAP_BAD_ARGUMENT;
    offset=(heap_u32)index*32768u;
    alias->selector=(heap_u16)(0x100u+r->logical_first*16u+index*8u);
    remaining=(heap_u32)r->paragraphs*16u;
    if (offset>=remaining) return HEAP_OK; /* Reserved tail stays absent. */
    remaining-=offset;
    alias->physical_base=s->physical_base+(heap_u32)r->physical_first*16u+offset;
    alias->limit=(heap_u16)(remaining>65536UL ? 65535UL : remaining-1u);
    alias->present=1;return HEAP_OK;
}
