#include "print.h"
#include "init.h"
#include "debug.h"
#include "string.h"
#include "memory.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "ioqueue.h"
#include "keyboard.h"
#include "process.h"

void k_thread_a(void* arg);
void k_thread_b(void* arg);
void u_prog_a();
void u_prog_b();
int test_var_a=1,test_var_b=0;

int main(void){
    put_str("I AM KERNEL\n");
    init_all();
    //asm volatile("sti");
    //ASSERT(1==2);
    //void* addr=get_kernel_pages(3);
    //put_str("\n get_kernel_pages start, vaddr is:");
    //put_int((uint32_t)addr);
    //put_str("\n");  
    //thread_start("kernel_thread_1", 31, k_thread_a, " argA");
    //thread_start("kernel_thread_2", 31, k_thread_b, " argB");
    //thread_start("kernel_thread_3", 8, k_thread_3, "arg3");
    process_execute(u_prog_a,"user_prog_a");
    //process_execute(u_prog_a,"user_prog_b");
    intr_enable();
    while(1){
        //intr_disable();
        //put_str("test_var_a ");
        //intr_enable();
        console_put_str("Main ");
        //console_put_int(test_var_a);
    }
    return 0;
}

void k_thread_a(void* arg){
    while(1)
    {
        /*
        enum intr_status old_status = intr_disable();
        while(!ioq_empty(&ioqueue))
        {
   	        console_put_str((char*)arg);
    	    char chr = ioq_getchar(&ioqueue);
   	        console_put_char(chr);
	    }
   	    intr_set_status(old_status);
        */
        console_put_str((char*)arg);
        console_put_int(test_var_a);
        console_put_char(' ');
        //++test_var_a;
    }
}

void k_thread_b(void* arg){
    while(1)
    {
        /*
        enum intr_status old_status = intr_disable();
        while(!ioq_empty(&ioqueue))
        {
   	        console_put_str((char*)arg);
    	    char chr = ioq_getchar(&ioqueue);
   	        console_put_char(chr);
	    }
   	    intr_set_status(old_status);
        */
        console_put_str((char*)arg);
        console_put_int(test_var_b);
        console_put_char(' ');
    }
}

void u_prog_a()
{
    while(1)
    {
    	++test_var_a;
        //console_put_str("u_prog ");
    }
}

void u_prog_b()
{
    while(1)
    {
    	//++test_var_b;
    }
}