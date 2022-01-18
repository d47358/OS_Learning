#include "thread.h"   //函数声明 各种结构体
#include "string.h"   //memset
#include "global.h"   //不清楚
#include "memory.h"   //分配页需要
#include "debug.h"
#define PG_SIZE 4096

struct task_struct* main_thread; //主线程PCB
struct list thread_ready_list;//就绪队列
struct list thread_all_list;//全部任务队列
static struct list_elem* thread_tag;//用于保存队列中的线程节点

extern void switch_to(struct task_struct* cur, struct task_struct* next);

//获取当前线程的PCB指针
struct task_struct* running_thread(){
    uint32_t esp;
    asm("mov %%esp,%0":"=g"(esp));
    return (struct task_struct*)(esp&0xfffff000);
}
static void kernel_thread(thread_func* function, void* func_arg){
    //首先开中断，避免时钟终端被屏蔽导致无法调度线程
    intr_enable();
    function(func_arg);
}
void init_thread(struct task_struct* pthread, char* name, int prio){
    memset(pthread, 0, sizeof(*pthread));
    strcpy(pthread->name,name);
    if(pthread==main_thread){
        pthread->status=TASK_RUNNING;
    }else{
        pthread->status=TASK_READY;
    }
    pthread->priority=prio;
    pthread->ticks=prio;
    pthread->elapsed_ticks=0;
    pthread->pgdir=NULL;
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
    ASSERT(!elem_find(&thread_ready_list,&thread->general_tag));
    list_append(&thread_ready_list,&thread->general_tag);
    ASSERT(!elem_find(&thread_all_list,&thread->all_list_tag));
    list_append(&thread_all_list,&thread->all_list_tag);
    return thread;
}

static void make_main_thread(){
    main_thread=running_thread();
    init_thread(main_thread,"main",31);
    ASSERT(!elem_find(&thread_all_list,&main_thread->all_list_tag));
    list_append(&thread_all_list,&main_thread->all_list_tag);