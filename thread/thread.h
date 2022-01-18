#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H
#include "stdint.h"

typedef void thread_func(void*);
//进程或线程状态
enum task_status{
    TASK_RUNNING, // 0
    TASK_READY,   // 1
    TASK_BLOCKED, // 2
    TASK_WAITING, // 3
    TASK_HANGING, // 4
    TASK_DIED     // 5
};

//中断栈，用于处理中断被切换的上下文环境储存
struct intr_struct{
    uint32_t vec_no; //中断号
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;
    //以下由cpu从低特权级到高特权级时压入
    uint32_t err_code;
    void (*eip) (void);        //这里声明了一个函数指针 
    uint32_t cs;
    uint32_t eflags;
    void* esp;
    uint32_t ss;
};

//线程栈
struct thread_stack{
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;

    void (*eip) (thread_func* func,void* func_arg); //和下面的相互照应 以ret 汇编代码进入kernel_thread函数调用
    
    void (*unused_retaddr);                         //占位数 在栈顶占返回地址的位置 因为是通过ret调用
    thread_func* function;                          //进入kernel_thread要调用的函数地址
    void* func_arg;	

};

//PCB
struct task_struct
{
    uint32_t* self_kstack;                          //pcb中的 kernel_stack 内核栈
    enum task_status status;                        //线程状态
    uint8_t priority;				      //特权级
    char name[16];
    uint32_t stack_magic;			      //越界检查  因为我们pcb上面的就是我们要用的栈了 到时候还要越界检查
};

#endif