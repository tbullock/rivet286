#include "startmon.h"
#ifndef PMTEST_STARTUP_HOST
static void *startup_address(startup_u16 offset) {return (void *)offset;}
#endif
static struct startup_state *startup_snapshot(void)
{
    return (struct startup_state *)startup_address(STARTUP_STATE);
}
static struct startup_context *startup_context(void)
{
    return (struct startup_context *)startup_address(STARTUP_CONTEXT);
}
startup_u16 startup_parse(const startup_u8 *tail,startup_u16 length,
    startup_u8 *filename,startup_u16 *name_size,startup_u16 *command_offset,
    startup_u16 *command_size)
{
    startup_u16 i=0,first,size;
    if(!tail || !filename || !name_size || !command_offset || !command_size || length>126)return 67;
    while(i<length && (tail[i]==' ' || tail[i]=='\t'))++i;
    if(length-i<2 || tail[i]!='/' || (tail[i+1u]!='R' && tail[i+1u]!='r'))return 0;
    if(length-i>2 && tail[i+2u]!=' ' && tail[i+2u]!='\t')return 0;
    i=(startup_u16)(i+2u);
    while(i<length && (tail[i]==' ' || tail[i]=='\t'))++i;
    first=i;
    while(i<length && tail[i]!=' ' && tail[i]!='\t') {
        if(tail[i]<=32 || tail[i]=='"')return 67;
        ++i;
    }
    size=(startup_u16)(i-first);
    if(!size || size>127 || tail[first]=='/')return 67;
    *command_offset=(startup_u16)(i<length ? i+1u : i);
    *command_size=(startup_u16)(length-*command_offset);
    if(*command_size>126)return 67;
    for(i=0;i<*command_size;++i)if(!tail[*command_offset+i])return 67;
    for(i=0;i<size;++i)filename[i]=tail[first+i];
    filename[size]=0;*name_size=size;return 1;
}
startup_u16 startup_kernel_prepare(void)
{
    struct startup_context *context=startup_context();
    startup_u8 *filename=(startup_u8 *)startup_address(STARTUP_FILENAME);
    startup_u8 *tail=(startup_u8 *)startup_address(0x81);
    startup_u16 name_size,offset,count,status,cs;
    context->profile=0;context->binding=0;startup_invalidate(startup_snapshot());
    status=startup_parse(tail,*(startup_u8 *)startup_address(0x80),filename,
        &name_size,&offset,&count);
    if(!status)return 0;
    if(status!=1) {context->prepare_error=67;return 67;}
    cs=startup_real_cs();context->binding=(startup_u16)(cs^0x5a31u);
    if(!context->binding)context->binding=1;
    status=startup_prepare(startup_snapshot(),context->binding,filename,name_size,
        tail+offset,count,
        cs,
        startup_phys_read,0);
    if(status) {context->prepare_error=status;context->binding=0;return 76;}
    /* Copy the exact filename before v3's extended header overwrites1540. */
    for(offset=0;offset<=name_size;++offset)
        ((startup_u8 *)startup_address(0x1540))[offset]=filename[offset];
    context->profile=1;return 0;
}
startup_u16 startup_kernel_publish(void)
{
    struct startup_context *context=startup_context();
    startup_u16 status;
    if(!context->profile)return 0;
    status=startup_publish(startup_snapshot(),context->binding);
    if(status) {context->publish_error=status;return 76;}
    return 0;
}
void startup_kernel_drop(void)
{
    startup_invalidate(startup_snapshot());startup_context()->active=0;
}
static startup_u16 startup_preflight(void *context,startup_u16 kind,
    startup_u32 pointer,startup_u32 count)
{
    struct pm_request request;
    (void)context;
    request.version=1;request.operation=PM_REQUEST_READ;request.flags=0;
    request.handle=0;request.kind=kind;request.origin=0;request.pointer=pointer;
    request.count=count;request.displacement=0;
    return validate_buffer(&request);
}
static startup_u16 startup_copy_chunk(void *context,startup_u16 kind,
    startup_u32 pointer,startup_u32 offset,const startup_u8 *source,startup_u16 count)
{
    struct pm_request request;
    (void)context;
    if(source!=PM_TRANSFER)return 8;
    request.version=1;request.operation=PM_REQUEST_READ;request.flags=0;
    request.handle=0;request.kind=kind;request.origin=0;request.pointer=pointer;
    request.count=count;request.displacement=0;
    return transfer(&request,offset,count,1);
}
static startup_u16 path_kernel_request(void);
startup_u16 startup_kernel_channel(void)
{
    struct startup_native_ops operations;
    pm_word channel=pm_read_word(PM_AT(0x611e));
    if(channel==0x0100)return kernel_request();
    if(channel==0x0500 || channel==0x0600)return path_kernel_request();
    if(channel!=0x0300)return PM_REQUEST_BAD_OPERATION;
    operations.context=0;operations.bounce=PM_TRANSFER;operations.bounce_bytes=512;
    operations.preflight=startup_preflight;operations.copy=startup_copy_chunk;
    return startup_native_request(startup_snapshot(),&operations,PM_RECORD,32);
}
