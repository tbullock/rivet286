/* Portable admission/publication policy; real-mode execution is separate. */
#include "execute.h"
static pm_word exec_word(const pm_byte *p) {
    return (pm_word)((pm_word)p[0]|((pm_word)p[1]<<8));
}
static pm_dword exec_dword(const pm_byte *p) {
    return (pm_dword)exec_word(p)|((pm_dword)exec_word(p+2)<<16);
}
static void exec_put(pm_byte *p,pm_word value) {
    p[0]=(pm_byte)value;p[1]=(pm_byte)(value>>8);
}
pm_word execute_dispatch(pm_byte *record,const struct execute_host *host) {
    pm_word op,binding,kind,split,count,tail,status,i;
    pm_dword pointer,size;
    struct execute_result returned;
    if(exec_word(record)!=EXECUTE_VERSION)return PM_REQUEST_BAD_VERSION;
    if(exec_word(record+4)||exec_dword(record+20)||exec_dword(record+24)||
       exec_word(record+28)||exec_word(record+30))return PM_REQUEST_BAD_RESERVED;
    op=exec_word(record+2);binding=exec_word(record+6);kind=exec_word(record+8);
    split=exec_word(record+10);pointer=exec_dword(record+12);size=exec_dword(record+16);
    if(op>1)return PM_REQUEST_BAD_OPERATION;
    if(!op){
        if(binding||kind||split||pointer||size)return PM_REQUEST_BAD_ARGUMENT;
        exec_put(record+24,3);return 0;
    }
    if(!binding||!host->admitted(host->context,binding))return PM_REQUEST_BAD_HANDLE;
    if(kind<PM_BUFFER_SELECTOR||kind>PM_BUFFER_LOGICAL||
       split<2||split>128||size<(pm_dword)split+1UL||
       size>(pm_dword)split+127UL)return PM_REQUEST_BAD_ARGUMENT;
    count=(pm_word)size;tail=(pm_word)(count-split-1);
    if((kind==PM_BUFFER_SELECTOR&&(pm_dword)(pm_word)pointer+size>65536UL)||
       pointer>0xffffffffUL-(size-1UL))return PM_REQUEST_BAD_EXTENT;
    status=host->access(host->context,kind,pointer,count);if(status)return status;
    status=host->copy(host->context,kind,pointer,count);if(status)return status;
    if(host->bounce[split-1]||host->bounce[count-1])return PM_REQUEST_BAD_ARGUMENT;
    for(i=0;i+1<split;++i)if(!host->bounce[i])return PM_REQUEST_BAD_ARGUMENT;
    for(i=split;i+1<count;++i)if(!host->bounce[i])return PM_REQUEST_BAD_ARGUMENT;
    /* No client bytes are used after this point; callback stages host storage. */
    returned.launched=0;returned.error=0;returned.child=0;
    status=host->execute(host->context,split,tail,&returned);
    if(status){
        /* A retryable transport failure cannot conceal a child side effect.
           Raw DOS launch failures use status0 plus the explicit error result. */
        if(status!=PM_REQUEST_BUSY||returned.launched||returned.error||returned.child)
            return EXECUTE_TERMINAL;
        return status;
    }
    if(!returned.launched){
        if(!returned.error||returned.child)return EXECUTE_TERMINAL;
        exec_put(record+28,returned.error);return PM_REQUEST_DOS_FAILURE;
    }
    if(returned.launched!=1||returned.error||(returned.child>>8)>3u)
        return EXECUTE_TERMINAL;
    exec_put(record+24,returned.child);return 0;
}
