#include "memory.h"
#include "print.h"
#include "string.h"
#define PAGE_SIZE 4096
//PCB最高处0xXXXXXfff以下是0特权级下使用的栈，因此栈顶是0xXXXXXfff+1
//内核加载时栈顶指向0xc009f000,PCB大小为fff,因此主线程PCB起始地址为0xc009efff-ffff=0xc009e000
//支持4页内存的位图，位图1位代表1页（4KB），4KB*(4*4*1024*8)=512MB,最大管理512MB内存
//因此位图地址为0xc009e000-0x4000=0xc009a000
#define MEM_BITMAP_BASE 0xc009a000
//内核堆起始虚拟地址，3GB以上跨过1MB(0x100000)
#define K_HEAP_START 0xc0100000

#define PDE_IDX(addr) ((addr&0xffc00000)>>22)
#define PTE_IDX(addr) ((addr&0x003ff000)>>12)//虚拟地址的前12位和中间12位

struct pool kernel_pool,user_pool;
struct virtual_addr kernel_vaddr;

//在虚拟地址池中申请pg_cnt个虚拟页
static void* vaddr_get(enum pool_flags pf,uint32_t pg_cnt){
    int vaddr_start=0, bits_idx_start=-1;
    uint32_t cnt=0;
    if(pf==PF_KERNEL){
        bits_idx_start=bitmap_scan(&kernel_vaddr.vaddr_bitmap,pg_cnt);
        ASSERT(bits_idx_start!=-1);
        if(bits_idx_start==-1)
            return NULL;
        while(cnt<pg_cnt){
            bitmap_set(&kernel_vaddr.vaddr_bitmap,bits_idx_start+cnt++,1);
        }
        vaddr_start=kernel_vaddr.vaddr_start+bits_idx_start*PAGE_SIZE;
    }else{
        //用户地址池
    }
    return (void*)vaddr_start;
}
//得到虚拟地址的pte指针
uint32_t* pte_ptr(uint32_t vaddr){
    uint32_t* pte=(uint32_t*)((0xffc00000)+((vaddr&0xffc00000)>>10)+PTE_IDX(vaddr)*4);
    return pte;
}
uint32_t* pde_ptr(uint32_t vaddr){
    uint32_t* pde=(uint32_t*)((0xfffff000)+PDE_IDX(vaddr)*4);
    return pde;
}
//分配1个物理页
static void* palloc(struct pool* mem_pool){
    int idx=bitmap_scan(&mem_pool->pool_bitmap,1);
    ASSERT(idx!=-1);
    if(idx==-1) return NULL;
    bitmap_set(&mem_pool->pool_bitmap,idx,1);
    uint32_t page_phyaddr=mem_pool->phy_addr_start+idx*PAGE_SIZE;
    return (void*)page_phyaddr;
}
//虚拟地址与物理地址的映射
static void page_table_add(void* _vaddr,void* _page_phyaddr){
    uint32_t vaddr=(uint32_t)_vaddr,page_phyaddr=(uint32_t)_page_phyaddr;
    uint32_t* pde=pde_ptr(vaddr);
    uint32_t* pte=pte_ptr(vaddr);
    if(*pde&0x00000001){
        //判断页目录表内的P位，页目录项是否存在
        ASSERT(!(*pte&0x00000001));//页表项不应该存在
        if(!(*pte&0x00000001)){
            //写入页表项
            *pte=(page_phyaddr|PG_RW_W|PG_US_S|PG_P_1);
        }else{
            PANIC("PTE REPEAT");
            *pte=(page_phyaddr|PG_RW_W|PG_US_S|PG_P_1);
        }
    }else{
        //页表项不存在，创建页表项
        uint32_t pde_phyaddr=(uint32_t)palloc(&kernel_pool);
        *pde=(pde_phyaddr|PG_RW_W|PG_P_1|PG_US_S);
        void* addr=(void*)((int)pte&0xfffff000);
        ASSERT(addr!=NULL);
        memset(addr,0,PAGE_SIZE);
        ASSERT(!(*pte&0x00000001));
        *pte=(page_phyaddr|PG_RW_W|PG_US_S|PG_P_1);
    }
}
//分配pg_cnt个页，返回起始虚拟地址
void* malloc_page(enum pool_flags pf,uint32_t pg_cnt){
    ASSERT(pg_cnt>0&&pg_cnt<3840);
    //1.通过vaddr_get在虚拟内存池中申请虚拟地址
    //2.通过palloc在内存池中分配物理地址
    //3.通过page_table_add建立映射关系
    void* vaddr_start=vaddr_get(pf,pg_cnt);
    ASSERT(vaddr_start!=NULL);
    if(vaddr_start==NULL){
        return NULL;
    }
    struct pool* mem_pool=pf==PF_KERNEL?&kernel_pool:&user_pool;
    uint32_t vaddr=(uint32_t)vaddr_start;
    while(pg_cnt-->0){
        void* page_phyaddr=palloc(mem_pool);
        ASSERT(page_phyaddr!=NULL);
        if(page_phyaddr==NULL){
            return NULL;
        }
        page_table_add((void*)vaddr,page_phyaddr);
        vaddr+=PAGE_SIZE;
    }
    return vaddr_start;
}

void* get_kernel_pages(uint32_t pg_cnt){
    void* vaddr=malloc_page(PF_KERNEL,pg_cnt);
    ASSERT(vaddr!=NULL);
    if(vaddr!=NULL){
        memset(vaddr,0,pg_cnt*PAGE_SIZE);
    }
    return vaddr;
}

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

