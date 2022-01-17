#include "print.h"
#include "init.h"
#include "thread.h"
void k_thread_1(void* arg);
int main(void){
    put_str("I AM KERNEL\n");
    init_all();
    //asm volatile("sti");
    //ASSERT(1==2);
    //void* addr=get_kernel_pages(3);
    //put_str("\n get_kernel_pages start, vaddr is:");
    //put_int((uint32_t)addr);
    //put_str("\n");
    thread_start("kernel_thread_1", 31, k_thread_1, "arg1");
    while(1);
    return 0;
}
void k_thread_1(void* arg){
    char* para=arg;
    while(1){
        put_str(para);
    }
}