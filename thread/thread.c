#include "thread.h"   //函数声明 各种结构体
#include "stdint.h"   //前缀
#include "string.h"   //memset
#include "global.h"   //不清楚
#include "memory.h"   //分配页需要

#define PG_SIZE 4096
static void kernel_thread(thread_func* function, void* func_arg){
    function(func_arg);
}
void init_thread(struct task_struct* pthread, char* name, int prio){
    memset(pthread, 0, sizeof(*pthread));
    strcpy(pthread->name,name);
    pthread->status=TASK_RUNNING;
    pthread->priority=prio;
    pthread->self_kstack=(uint32_t*)((uint32_t)pthread+PG_SIZE);
    pthread->stack_magic=0x19980906;
}

void thread_create(struct task_struct* pthread, thread_func function, void* func_arg){
    pthread->self_kstack-=sizeof(struct intr_struct);//预留中断栈空间
    pthread->self_kstack-=sizeof(struct thread_stack);//预留线程栈空间
    struct thread_stack* kthread_stack=(struct thread_stack*)pthread->self_kstack;
    kthread_stack->ebp=kthread_stack->ebx=kthread_stack->edi=kthread_stack->esi=0;
    kthread_stack->eip=kernel_thread;
    kthread_stack->function=function;
    kthread_stack->func_arg=func_arg;
}

struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg){
    struct task_struct* thread=get_kernel_pages(1);
    init_thread(thread, name, prio);
    thread_create(thread,function,func_arg);
    asm volatile("movl %0,%%esp; pop %%ebp; pop %%ebx; pop %%edi; pop %%esi; ret" : : "g"(thread->self_kstack) :"memory"); //栈顶的位置为 thread->self_kstack 
    return thread;
}