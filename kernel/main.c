#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "ioqueue.h"
#include "keyboard.h"
void k_thread_1(void* arg);
void k_thread_2(void* arg);
void k_thread_3(void* arg);
int main(void){
    put_str("I AM KERNEL\n");
    init_all();
    //asm volatile("sti");
    //ASSERT(1==2);
    //void* addr=get_kernel_pages(3);
    //put_str("\n get_kernel_pages start, vaddr is:");
    //put_int((uint32_t)addr);
    //put_str("\n");  
    thread_start("kernel_thread_1", 31, k_thread_1, " A_");
    thread_start("kernel_thread_2", 31, k_thread_2, " B_");
    //thread_start("kernel_thread_3", 8, k_thread_3, "arg3");
    intr_enable();
    while(1){
        //intr_disable();
        //put_str("Main ");
        //intr_enable();
        //console_put_str("Main ");
    }
    return 0;
}

void k_thread_1(void* arg){
    while(1)
    {
        enum intr_status old_status = intr_disable();
        while(!ioq_empty(&ioqueue))
        {
   	        console_put_str((char*)arg);
    	    char chr = ioq_getchar(&ioqueue);
   	        console_put_char(chr);
	    }
   	    intr_set_status(old_status);
    }
}

void k_thread_2(void* arg){
    while(1)
    {
        enum intr_status old_status = intr_disable();
        while(!ioq_empty(&ioqueue))
        {
   	        console_put_str((char*)arg);
    	    char chr = ioq_getchar(&ioqueue);
   	        console_put_char(chr);
	    }
   	    intr_set_status(old_status);
    }
}

void k_thread_3(void* arg){
    char* para=arg;
    while(1){
        //intr_disable();
        //put_str((char*)para);
        //intr_enable();
        console_put_str((char*)arg);
    }
}