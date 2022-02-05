#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "debug.h"
#include "interrupt.h"
#include "print.h"
#include "memory.h"
#include "process.h"
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
    pthread->self_kstack-=sizeof(struct intr_stack);//预留中断栈空间
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
    init_thread(main_thread,"main",1);
    ASSERT(!elem_find(&thread_all_list,&main_thread->all_list_tag));
    list_append(&thread_all_list,&main_thread->all_list_tag);
}

void schedule(){
    ASSERT(intr_get_status()==INTR_OFF);
    struct task_struct* cur=running_thread();
    if(cur->status==TASK_RUNNING){
        ASSERT(!elem_find(&thread_ready_list,&cur->general_tag));
        list_append(&thread_ready_list,&cur->general_tag);
        cur->ticks=cur->priority;
        cur->status=TASK_READY;
    }else{
        //线程需要发生某些事件才能继续，不放入就绪队列
    }
    ASSERT(!list_empty(&thread_ready_list));
    thread_tag=list_pop(&thread_ready_list);
    struct task_struct* next=elem2entry(struct task_struct,general_tag,thread_tag);
    next->status=TASK_RUNNING;
    process_activate(next);
    switch_to(cur,next);
}

void thread_init(){
    put_str("thread_init start\n");
    list_init(&thread_ready_list);
    list_init(&thread_all_list);
    make_main_thread();
    put_str("thread_init done\n");
}

void thread_block(enum task_status stat)
{
    //设置block状态的参数必须是下面三个以下的
    ASSERT(((stat == TASK_BLOCKED) || (stat == TASK_WAITING) || stat == TASK_HANGING));
    
    enum intr_status old_status = intr_disable();			 //关中断
    struct task_struct* cur_thread = running_thread();		 
    cur_thread->status = stat;					 //把状态重新设置
    
    //调度器切换其他进程了 而且由于status不是running 不会再被放到就绪队列中
    schedule();	
    				
    //被切换回来之后再进行的指令了
    intr_set_status(old_status);
}

//由锁拥有者来执行的 善良者把原来自我阻塞的线程重新放到队列中
void thread_unblock(struct task_struct* pthread)
{
    enum intr_status old_status = intr_disable();
    ASSERT(((pthread->status == TASK_BLOCKED) || (pthread->status == TASK_WAITING) || (pthread->status == TASK_HANGING)));
    if(pthread->status != TASK_READY)
    {
    	//被阻塞线程 不应该存在于就绪队列中）
    	ASSERT(!elem_find(&thread_ready_list,&pthread->general_tag));
    	if(elem_find(&thread_ready_list,&pthread->general_tag))
    	    PANIC("thread_unblock: blocked thread in ready_list\n"); //debug.h中定义过
    	
    	//让阻塞了很久的任务放在就绪队列最前面
    	list_push(&thread_ready_list,&pthread->general_tag);
    	
    	//状态改为就绪态
    	pthread->status = TASK_READY;
    }
    intr_set_status(old_status);
}
