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
#include "syscall-init.h"
#include "syscall.h"
#include "stdio.h"
void k_thread_a(void* arg);
void k_thread_b(void* arg);
void u_prog_a();
void u_prog_b();
//int test_var_a=1,test_var_b=0;
//int prog_a_pid = 0, prog_b_pid = 0;

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
    //process_execute(u_prog_a,"user_prog_a");
    
    intr_enable();
    //console_put_str("main_pid:0x");
    //console_put_int(sys_getpid());
    //console_put_char('\n');
    
    thread_start("kernel_thread_a", 31, k_thread_a, " threadA: 0x");
    thread_start("kernel_thread_b", 31, k_thread_b, " threadB: 0x");
    process_execute(u_prog_b,"user_prog_a");
    process_execute(u_prog_a,"user_prog_b");
    
    
    while(1){
        
        //intr_disable();
        //put_str("test_var_a ");
        //intr_enable();
        //console_put_str("Main ");
        //console_put_int(test_var_a);
    }
    return 0;
}

void k_thread_a(void* arg){
    void* addr1 = sys_malloc(256);
	void* addr2 = sys_malloc(255);
	void* addr3 = sys_malloc(254);
	console_put_str(" thread_a malloc addr:0x");
	console_put_int((int)addr1);
	console_put_char(',');
	console_put_int((int)addr2);
	console_put_char(',');
	console_put_int((int)addr3);
	console_put_char('\n');
	int cpu_delay = 100000;
	while(cpu_delay-->0);
	sys_free(addr1);
	sys_free(addr2);
	sys_free(addr3);
	while(1);
}

void k_thread_b(void* arg){
    void* addr1 = sys_malloc(256);
	void* addr2 = sys_malloc(255);
	void* addr3 = sys_malloc(254);
	console_put_str(" thread_b malloc addr:0x");
	console_put_int((int)addr1);
	console_put_char(',');
	console_put_int((int)addr2);
	console_put_char(',');
	console_put_int((int)addr3);
	console_put_char('\n');
	int cpu_delay = 100000;
	while(cpu_delay-->0);
	sys_free(addr1);
	sys_free(addr2);
	sys_free(addr3);
	while(1);
}

void u_prog_a() {
    void* addr1 = malloc(256);
    uint32_t a1 = (uint32_t) addr1;
    printf(" prog_a malloc addr:0x%x%c", (int)addr1, '\n');
	void* addr2 = malloc(255);
    uint32_t a2 = (uint32_t) addr2;
    printf(" prog_a malloc addr:0x%x%c", (int)addr2, '\n');
	void* addr3 = malloc(254);
    uint32_t a3 = (uint32_t) addr3;
	printf(" prog_a malloc addr:0x%x%c", (int)addr3, '\n');
	int cpu_delay = 100000;
	while(cpu_delay-->0);
	free(addr1);
    printf(" prog_a free addr:0x%x%c", a1, '\n');
	free(addr2);
    printf(" prog_a free addr:0x%x%c", a2, '\n');
	free(addr3);
    printf(" prog_a free addr:0x%x%c", a3, '\n');
	while(1);
}

void u_prog_b() {
    void* addr1 = malloc(256);
    uint32_t a1 = (uint32_t) addr1;
    printf(" prog_b malloc addr:0x%x%c", (int)addr1, '\n');
	void* addr2 = malloc(255);
    uint32_t a2 = (uint32_t) addr2;
    printf(" prog_b malloc addr:0x%x%c", (int)addr2, '\n');
	void* addr3 = malloc(254);
    uint32_t a3 = (uint32_t) addr3;
	printf(" prog_b malloc addr:0x%x%c", (int)addr3, '\n');
	int cpu_delay = 100000;
	while(cpu_delay-->0);
	free(addr1);
    printf(" prog_b free addr 0x%x%c", a1, '\n');
	free(addr2);
    printf(" prog_b free addr0x%x%c", a2, '\n');
	free(addr3);
    printf(" prog_b free addr0x%x%c", a3, '\n');
	while(1);
}