#include "memory.h"
#include "print.h"
#define PAGE_SIZE 4096
//PCB最高处0xXXXXXfff以下是0特权级下使用的栈，因此栈顶是0xXXXXXfff+1
//内核加载时栈顶指向0xc009f000,PCB大小为fff,因此主线程PCB起始地址为0xc009efff-ffff=0xc009e000
//支持4页内存的位图，位图1位代表1页（4KB），4KB*(4*4*1024*8)=512MB,最大管理512MB内存
//因此位图地址为0xc009e000-0x4000=0xc009a000
#define MEM_BITMAP_BASE 0xc009a000
//内核堆起始虚拟地址，3GB以上跨过1MB(0x100000)
#define K_HEAP_START 0xc0100000

struct pool kernel_pool,user_pool;
struct virtual_addr kernel_vaddr;

static void mem_pool_init(uint32_t all_mem){
    put_str("mem_pool init start\n");
    uint32_t page_table_size=PAGE_SIZE*256;
    uint32_t used_mem=page_table_size+0x100000;
    uint32_t free_mem=all_mem-used_mem;
    //总页数
    uint16_t all_free_pages=free_mem/PAGE_SIZE;
    //内核、用户页数
    uint16_t kernel_free_pages=all_free_pages/2;
    uint16_t user_free_pages=all_free_pages-kernel_free_pages;
    //位图长度
    uint32_t kernel_bitmap_length=kernel_free_pages/8;
    uint32_t user_bitmap_length=user_free_pages/8;
    //内存池地址
    uint32_t kernel_pool_start=used_mem;
    uint32_t user_pool_start=used_mem+kernel_free_pages*PAGE_SIZE;
    
    kernel_pool.phy_addr_start=kernel_pool_start;
    user_pool.phy_addr_start=user_pool_start;
    kernel_pool.pool_size=kernel_free_pages*PAGE_SIZE;
    user_pool.pool_size=user_free_pages*PAGE_SIZE;
    kernel_pool.pool_bitmap.btmp_bytes_len=kernel_bitmap_length;
    user_pool.pool_bitmap.btmp_bytes_len=user_bitmap_length;

    kernel_pool.pool_bitmap.bits=(void*)MEM_BITMAP_BASE;
    user_pool.pool_bitmap.bits=(void*)(MEM_BITMAP_BASE+kernel_bitmap_length);

    put_str("   kernel_pool_bitmap start:");
    put_int((int)kernel_pool.pool_bitmap.bits);
    put_str(" kernel_pool_phy_addr start:");
    put_int(kernel_pool.phy_addr_start);
    put_str("\n");
    put_str("user_pool_bitmap start:");
    put_int((int)user_pool.pool_bitmap.bits);
    put_str(" user_pool_phy_addr start:");
    put_int(user_pool.phy_addr_start);
    put_str("\n");

    //将位图置0
    bitmap_init(&kernel_pool.pool_bitmap);
    bitmap_init(&user_pool.pool_bitmap);

    //内核虚拟地址位图
    kernel_vaddr.vaddr_bitmap.btmp_bytes_len=kernel_bitmap_length;
    //虚拟地址位图定义在内存池之外
    kernel_vaddr.vaddr_bitmap.bits=(void*)(MEM_BITMAP_BASE+kernel_bitmap_length+user_bitmap_length);

    kernel_vaddr.vaddr_start=K_HEAP_START;
    bitmap_init(&kernel_vaddr.vaddr_bitmap);
    put_str("   mem_pool_init done\n");
}

void mem_init(){
    put_str("mem_init start\n");
    uint32_t all_mem=*(uint32_t*)(0xb00);
    mem_pool_init(all_mem);
    put_str("mem_init done\n");
}