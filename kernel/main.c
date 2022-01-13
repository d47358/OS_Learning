#include "print.h"
#include "init.h"
#include "debug.h"
#include "memory.h"
int main(void){
    put_str("I AM KERNEL\n");
    init_all();
    //asm volatile("sti");
    //ASSERT(1==2);
    void* addr=get_kernel_pages(3);
    put_str("\n get_kernel_pages start, vaddr is:");
    put_int((uint32_t)addr);
    put_str("\n");
    while(1);
    return 0;
}