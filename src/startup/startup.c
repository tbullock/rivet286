#include "startup.h"
void startup_invalidate(struct startup_state *state)
{
    if(state) {state->meta.binding=0;state->meta.prepared=0;state->meta.ready=0;}
}
static startup_u16 read_byte(startup_reader reader,void *context,
    startup_u32 address,startup_u8 *value)
{
    if(address>=0x100000UL || reader(context,address,value))return STARTUP_SOURCE;
    return STARTUP_OK;
}
static startup_u16 read_word(startup_reader reader,void *context,
    startup_u32 address,startup_u16 *value)
{
    startup_u8 low,high;
    if(read_byte(reader,context,address,&low) ||
       read_byte(reader,context,address+1UL,&high))return STARTUP_SOURCE;
    *value=(startup_u16)((startup_u16)low|((startup_u16)high<<8));
    return STARTUP_OK;
}
startup_u16 startup_prepare(struct startup_state *state,startup_u16 binding,
    const startup_u8 *filename,startup_u16 filename_size,
    const startup_u8 *command,startup_u16 command_size,startup_u16 psp,
    startup_reader reader,void *context)
{
    startup_u16 i,segment,owner,paragraphs,limit,env_size=0;
    startup_u32 address,mcb,bytes;
    startup_u8 signature;
    startup_u8 *clear;
    if(!state)return STARTUP_ARGUMENT;
    clear=(startup_u8 *)state;
    for(i=0;i<(startup_u16)sizeof(*state);++i)clear[i]=0;
    if(!binding || !filename || !filename_size || filename_size>127 ||
       command_size>126 || (command_size && !command) || !reader)
        return STARTUP_ARGUMENT;
    for(i=0;i<filename_size;++i)
        if(filename[i]<=32 || filename[i]=='"')return STARTUP_ARGUMENT;
    for(i=0;i<command_size;++i)
        if(!command[i])return STARTUP_ARGUMENT;
    address=(startup_u32)psp<<4;
    if(address>0x100000UL-0x2eUL ||
       read_word(reader,context,address+0x2cUL,&segment))return STARTUP_SOURCE;
    if(!segment)env_size=2;
    else {
        address=(startup_u32)segment<<4;mcb=address-16UL;
        if(read_byte(reader,context,mcb,&signature) ||
           (signature!='M' && signature!='Z') ||
           read_word(reader,context,mcb+1UL,&owner) || !owner ||
           read_word(reader,context,mcb+3UL,&paragraphs) ||
           !paragraphs || paragraphs>4096)return STARTUP_SOURCE;
        bytes=(startup_u32)paragraphs*16UL;
        if(address>0x100000UL-bytes)return STARTUP_SOURCE;
        limit=(startup_u16)(bytes>2048UL ? 2048UL : bytes);
        for(i=0;i<limit;++i) {
            if(read_byte(reader,context,address+i,&state->env[i]))return STARTUP_SOURCE;
            if(i && !state->env[i] && !state->env[i-1u]) {
                env_size=(startup_u16)(i+1u);break;
            }
        }
        if(!env_size)return STARTUP_SOURCE;
    }
    for(i=0;i<filename_size;++i)state->executable[i]=filename[i];
    for(i=0;i<command_size;++i)state->command[i]=command[i];
    state->meta.env_size=env_size;
    state->meta.cmd_size=(startup_u16)(command_size+1u);
    state->meta.exe_size=(startup_u16)(filename_size+1u);
    state->meta.binding=binding;state->meta.prepared=1;
    return STARTUP_OK;
}
startup_u16 startup_publish(struct startup_state *state,startup_u16 binding)
{
    if(!state || !state->meta.prepared)return STARTUP_UNAVAILABLE;
    if(!binding || state->meta.binding!=binding)return STARTUP_BINDING;
    state->meta.ready=1;return STARTUP_OK;
}
static startup_u16 item_view(const struct startup_state *state,startup_u16 binding,
    startup_u16 item,const startup_u8 **data,startup_u16 *size)
{
    if(!state || !state->meta.prepared || !state->meta.ready)return STARTUP_UNAVAILABLE;
    if(!binding || binding!=state->meta.binding)return STARTUP_BINDING;
    if(item==STARTUP_ENV) {*data=state->env;*size=state->meta.env_size;}
    else if(item==STARTUP_COMMAND) {*data=state->command;*size=state->meta.cmd_size;}
    else if(item==STARTUP_EXECUTABLE) {*data=state->executable;*size=state->meta.exe_size;}
    else return STARTUP_ITEM;
    return STARTUP_OK;
}
startup_u16 startup_size(const struct startup_state *state,startup_u16 binding,
    startup_u16 item,startup_u16 *size)
{
    const startup_u8 *data;
    startup_u16 result,value;
    if(!size)return STARTUP_ARGUMENT;
    result=item_view(state,binding,item,&data,&value);
    if(!result)*size=value;
    return result;
}
startup_u16 startup_copy(const struct startup_state *state,startup_u16 binding,
    startup_u16 item,startup_u8 *destination,startup_u16 size)
{
    const startup_u8 *data,*owner=(const startup_u8 *)state;
    startup_u16 result,value,i;
    result=item_view(state,binding,item,&data,&value);
    if(result)return result;
    if(size!=value)return STARTUP_SIZE;
    if(!destination)return STARTUP_ARGUMENT;
    /* Equality comparisons are portable even for distinct objects. Reject
       aliasing the immutable state without ordering unrelated pointers. */
    for(i=0;i<(startup_u16)sizeof(*state);++i)
        if(destination==owner+i)return STARTUP_ARGUMENT;
    for(i=0;i<size;++i)
        if(destination+i==owner)return STARTUP_ARGUMENT;
    for(i=0;i<size;++i)destination[i]=data[i];
    return STARTUP_OK;
}

startup_u16 startup_read(const struct startup_state *state,startup_u16 binding,
    startup_u16 item,startup_u16 offset,startup_u8 *destination,startup_u16 count)
{
    const startup_u8 *data,*owner=(const startup_u8 *)state;
    startup_u16 result,size,i;
    result=item_view(state,binding,item,&data,&size);
    if(result)return result;
    if(!count || count>512 || offset>size || count>size-offset)return STARTUP_SIZE;
    if(!destination)return STARTUP_ARGUMENT;
    for(i=0;i<(startup_u16)sizeof(*state);++i)
        if(destination==owner+i)return STARTUP_ARGUMENT;
    for(i=0;i<count;++i)
        if(destination+i==owner)return STARTUP_ARGUMENT;
    for(i=0;i<count;++i)destination[i]=data[offset+i];
    return STARTUP_OK;
}
