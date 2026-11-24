#include "segments.h"
static int active(heap_u16 value,heap_u16 first,heap_u16 end) {
    /* A same-index LDT selector is not one of this owner's GDT aliases. */
    heap_u16 index=(heap_u16)(value&0xfff8u);
    return !(value&4u) && index>=first && index<end;
}
heap_u16 heap_release_preflight(const struct heap_state *state,heap_u16 origin,
    const struct heap_client_segments *client) {
    heap_u16 reference,error,end;
    if (!client) return HEAP_BAD_ARGUMENT;
    error=heap_find_live(state,origin,&reference);
    if (error) return error;
    end=(heap_u16)(origin+state->records[reference-1u].slots*16u);
    if (active(client->ds,origin,end) || active(client->es,origin,end) ||
        active(client->ss,origin,end)) return HEAP_ACTIVE_SEGMENT;
    return HEAP_OK;
}
