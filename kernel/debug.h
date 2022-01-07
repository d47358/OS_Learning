#ifndef __KERNEL_DEBUG_H
#define __LERNEL_DEBUG_H
void panic_spin(char* filename,int line,const char* func,const char*condition);
//C语言中的__FILE__等用以指示本行语句所在源文件的文件名
#define PANIC(...) panic_spin(__FILE__,__LINE__,__func__,__VA_ARGS__)
#ifndef NDEBUG
#define ASSERT(CONDITION) \
if(CONDITION){} \
else { \
    PANIC(#CONDITION); \
}
#endif
#endif