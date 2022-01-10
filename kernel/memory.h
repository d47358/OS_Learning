#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "stdint.h"
#include "bitmap.h"

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