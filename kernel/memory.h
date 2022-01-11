#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "stdint.h"
#include "bitmap.h"
#define NULL (void*)0
enum pool_flags{
    PF_KERNEL=1,
    PF_USER=2
};
#define PG_P_1 1
#define PG_P_0 0 //页表项和页目录项是否存在
#define PG_RW_R 0 //R/W位
#define PG_RW_W 2
#define PG_US_S 0 //U/S位
#define PG_US_U 4
//虚拟地址池
struct virtual_addr
{
    struct bitmap vaddr_bitmap;
    uint32_t vaddr_start;
};
//内存池
struct pool{
    struct bitmap pool_bitmap; //内存池用到的位图
    uint32_t phy_addr_start;  //本内存池管理的物理内存的起始地址
    uint32_t pool_size;

};

void mem_init();

#endif