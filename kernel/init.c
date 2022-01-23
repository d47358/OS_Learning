#include "init.h"
#include "print.h"
#include "interrupt.h"
#include "memory.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"
/*负责初始化所有模块 */
void init_all() {
   put_str("init_all\n");
   idt_init();	     // 初始化中断
   mem_init();
   thread_init();
   timer_init();
   console_init();
   keyboard_init();
}

