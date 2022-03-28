#include "memory.h"
#include "stdint.h"
#include "print.h"
#include "bitmap.h"
#include "debug.h"
#include "string.h"
#include "global.h"
#include "sync.h"
#include "thread.h"
#include "list.h"
#include "interrupt.h"

#define PAGE_SIZE 4096
//PCB最高处0xXXXXXfff以下是0特权级下使用的栈，因此栈顶是0xXXXXXfff+1
//内核加载时栈顶指向0xc009f000,PCB大小为fff,因此主线程PCB起始地址为0xc009efff-ffff=0xc009e000
//支持4页内存的位图，位图1位代表1页（4KB），4KB*(4*4*1024*8)=512MB,最大管理512MB内存
//因此位图地址为0xc009e000-0x4000=0xc009a000
#define MEM_BITMAP_BASE 0xc009a000
//内核堆起始虚拟地址，3GB以上跨过1MB(0x100000)
#define K_HEAP_START 0xc0100000

struct pool{
    struct bitmap pool_bitmap; //内存池用到的位图
    uint32_t phy_addr_start;  //本内存池管理的物理内存的起始地址
    uint32_t pool_size;
    struct lock lock;
};

#define PDE_IDX(addr) ((addr&0xffc00000)>>22)
#define PTE_IDX(addr) ((addr&0x003ff000)>>12)//虚拟地址的前10位和中间10位

struct mem_block_desc k_block_desc[DESC_CNT]; //内核内存块描述符数组
struct pool kernel_pool,user_pool;
struct virtual_addr kernel_vaddr;



//在虚拟地址池中申请pg_cnt个虚拟页
static void* vaddr_get(enum pool_flags pf,uint32_t pg_cnt){
    int vaddr_start=0, bits_idx_start=-1;
    uint32_t cnt=0;
    
    if(pf==PF_KERNEL){
        //lock_acquire(&kernel_vaddr.lock);
        bits_idx_start=bitmap_scan(&kernel_vaddr.vaddr_bitmap,pg_cnt);
        ASSERT(bits_idx_start!=-1);
        if(bits_idx_start==-1){
            //lock_release(&kernel_vaddr.lock);
            return NULL;
        }
        while(cnt<pg_cnt){
            bitmap_set(&kernel_vaddr.vaddr_bitmap,bits_idx_start+cnt++,1);
        }
        //lock_release(&kernel_vaddr.lock);
        vaddr_start=kernel_vaddr.vaddr_start+bits_idx_start*PAGE_SIZE;
    }else{
        //用户地址池
        struct task_struct* cur=running_thread();
        //lock_acquire(&cur->userprog_vaddr.lock);
        bits_idx_start=bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap,pg_cnt);
        if(bits_idx_start==-1){
            //lock_release(&cur->userprog_vaddr.lock);
            return NULL;
        } 
        while(cnt<pg_cnt){
            bitmap_set(&cur->userprog_vaddr.vaddr_bitmap,bits_idx_start+cnt++,1);
        }
        //lock_release(&cur->userprog_vaddr.lock);
        vaddr_start=cur->userprog_vaddr.vaddr_start+bits_idx_start*PG_SIZE;
        ASSERT((uint32_t)vaddr_start<(0xc0000000-PG_SIZE));
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
        //ASSERT(!(*pte&0x00000001));//页表项不应该存在
        if(!(*pte&0x00000001)){
            //写入页表项
            *pte=(page_phyaddr|PG_US_U | PG_RW_W | PG_P_1);
        }else{
            PANIC("PTE REPEAT");
            *pte=(page_phyaddr|PG_US_U | PG_RW_W | PG_P_1);
        }
    }else{
        //页表项不存在，创建页表项
        uint32_t pde_phyaddr=(uint32_t)palloc(&kernel_pool);
        *pde=(pde_phyaddr|PG_US_U | PG_RW_W | PG_P_1);
        void* addr=(void*)((int)pte&0xfffff000);
        ASSERT(addr!=NULL);
        memset(addr,0,PAGE_SIZE);
        ASSERT(!(*pte&0x00000001));
        *pte=(page_phyaddr|PG_US_U | PG_RW_W | PG_P_1);
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
    lock_acquire(&kernel_pool.lock);
    void* vaddr=malloc_page(PF_KERNEL,pg_cnt);
    ASSERT(vaddr!=NULL);
    if(vaddr!=NULL){
        memset(vaddr,0,pg_cnt*PAGE_SIZE);
    }
    lock_release(&kernel_pool.lock);
    return vaddr;
}

void* get_user_pages(uint32_t pg_cnt){
    lock_acquire(&user_pool.lock);
    void* vaddr=malloc_page(PF_USER,pg_cnt);
    ASSERT(vaddr!=NULL);
    memset(vaddr,0,pg_cnt*PAGE_SIZE);
    lock_release(&user_pool.lock);
    return vaddr;
}

void* get_a_page(enum pool_flags pf,uint32_t vaddr)
{
    struct pool* mem_pool = (pf == PF_KERNEL) ? &kernel_pool : &user_pool;
    lock_acquire(&mem_pool->lock);
    
    struct task_struct* cur = running_thread();
    int32_t bit_idx = -1;
    
    //虚拟地址位图置1
    if(cur->pgdir != NULL && pf == PF_USER)
    {
    	bit_idx = (vaddr - cur->userprog_vaddr.vaddr_start) / PG_SIZE;
    	ASSERT(bit_idx > 0);
    	bitmap_set(&cur->userprog_vaddr.vaddr_bitmap,bit_idx,1);
    }
    else if(cur->pgdir == NULL && pf == PF_KERNEL) 
    {
    	bit_idx = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
    	ASSERT(bit_idx > 0);
    	bitmap_set(&kernel_vaddr.vaddr_bitmap,bit_idx,1);
    }
    else
    	PANIC("get_a_page:not allow kernel alloc userspace or user alloc kernelspace by get_a_page");
    	
    void* page_phyaddr = palloc(mem_pool);
    if(page_phyaddr == NULL)
    	return NULL;
    page_table_add((void*)vaddr,page_phyaddr);
    lock_release(&mem_pool->lock);
    return (void*)vaddr;
}

uint32_t addr_v2p(uint32_t vaddr)
{
    uint32_t* pte = pte_ptr(vaddr);
    return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
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
    lock_init(&kernel_pool.lock);
    lock_init(&user_pool.lock);
    put_str("   mem_pool_init done\n");
}

void mem_init(){
    put_str("mem_init start\n");
    uint32_t all_mem=*(uint32_t*)(0xb00);
    mem_pool_init(all_mem);
    block_desc_init(k_block_desc);
    put_str("mem_init done\n");
}

void block_desc_init(struct mem_block_desc* desc_arry) {
    uint16_t desc_idx, block_size = 16;
    for (desc_idx = 0; desc_idx < DESC_CNT; ++desc_idx) {
        desc_arry[desc_idx].block_size = block_size;
        desc_arry[desc_idx].blocks_per_arena = (PG_SIZE - sizeof(struct arena)) / block_size; // arena中内存数量
        list_init(&desc_arry[desc_idx].free_list);
        block_size *= 2;
    }
}

static struct mem_block* arena2block(struct arena* a, uint32_t idx) {
    return (struct mem_block*) ((uint32_t)a + sizeof(struct arena) + idx * a->desc->block_size);
}

static struct arena* block2arena(struct mem_block* b) {
    return (struct arena*) ((uint32_t)b & 0xfffff000);
}

void* sys_malloc(uint32_t size) {
    enum pool_flags PF;
    struct pool* mem_pool;
    uint32_t pool_size;
    struct mem_block_desc* descs;
    struct task_struct* cur_thread = running_thread();
    if (cur_thread->pgdir == NULL) { //内核线程
        PF = PF_KERNEL;
        pool_size = kernel_pool.pool_size;
        mem_pool = &kernel_pool;
        descs = k_block_desc;
    } else {
        PF = PF_USER;
        pool_size = user_pool.pool_size;
        mem_pool = &user_pool;
        descs = cur_thread->u_block_desc;
    }
    if (size < 0 || size > pool_size)
        return NULL;
    struct arena* a;
    struct mem_block* b;
    lock_acquire(&mem_pool->lock);

    //超过1024字节，分配页框
    if (size > 1024) {
        uint32_t pg_cnt = DIV_ROUND_UP(size + sizeof(struct arena), PG_SIZE);
        a = malloc_page(PF, pg_cnt);
        if (a != NULL){
            memset(a, 0, pg_cnt * PG_SIZE);
            a->desc = NULL;
            a->cnt = pg_cnt;
            a->large = true;
            lock_release(&mem_pool->lock);
            return (void*) (a + 1);
        } else {
            lock_release(&mem_pool->lock);
            return NULL;
        }
    } else {
        uint8_t desc_idx;
        for (desc_idx = 0; desc_idx < DESC_CNT; ++desc_idx) {
            if (size <= descs[desc_idx].block_size) 
                break;
        }
        if (list_empty(&descs[desc_idx].free_list)) {
            a = malloc_page(PF, 1);
            if (a == NULL) {
                lock_release(&mem_pool->lock);
                return NULL;
            }
            memset(a, 0, PG_SIZE);

            a->desc = &descs[desc_idx];
            a->large = false;
            a->cnt = descs[desc_idx].blocks_per_arena;
            uint32_t block_idx;
            enum intr_status old_status = intr_disable();

            for (block_idx = 0; block_idx < descs[desc_idx].blocks_per_arena; ++block_idx) {
                b = arena2block(a, block_idx);
                ASSERT(!elem_find(&a->desc->free_list, &b->free_elem));
                list_append(&a->desc->free_list, &b->free_elem);
            }
            intr_set_status(old_status);
        }
        b = elem2entry(struct mem_block, free_elem, list_pop(&(descs[desc_idx].free_list)));
        memset(b, 0, descs[desc_idx].block_size);

        a = block2arena(b);
        --a->cnt;
        lock_release(&mem_pool->lock);
        return (void*)b;
    }
}

void pfree(uint32_t pg_phy_addr) {
    struct pool* mem_pool;
    uint32_t bit_idx = 0;
    if (pg_phy_addr >= user_pool.phy_addr_start) {
        mem_pool = &user_pool;
        bit_idx = (pg_phy_addr - user_pool.phy_addr_start) / PG_SIZE;
    } else {
        mem_pool = &kernel_pool;
        bit_idx = (pg_phy_addr - kernel_pool.phy_addr_start) / PG_SIZE;
    }
    bitmap_set(&mem_pool->pool_bitmap, bit_idx, 0);
}

//释放页表中的映射
static void page_table_pte_remove(uint32_t vaddr) {
    uint32_t* pte = pte_ptr(vaddr);
    *pte &= ~PG_P_1;
    asm volatile ("invlpg %0" :: "m"(vaddr) : "memory");
}

//释放虚拟地址池
static void vaddr_remove(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt) {
    uint32_t bit_idx_start = 0, vaddr = (uint32_t) _vaddr, cnt = 0;
    if (pf == PF_KERNEL) {
        bit_idx_start = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
        while (cnt < pg_cnt) {
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 0);
        }
    } else {
        struct task_struct* cur = running_thread();
        bit_idx_start = (vaddr - cur->userprog_vaddr.vaddr_start) / PG_SIZE;
        while (cnt < pg_cnt) {
            bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 0);
        }
    }
}

//释放vaddr起始的cnt个物理页
void mfree_page(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt) {
    uint32_t pg_phy_addr;
    uint32_t vaddr = (uint32_t) _vaddr, page_cnt = 0;
    ASSERT(pg_cnt > 0 && vaddr % PG_SIZE == 0);
    pg_phy_addr = addr_v2p(vaddr);

    ASSERT(pg_phy_addr % PG_SIZE == 0 && pg_phy_addr >= 0x102000);
    while (page_cnt < pg_cnt) {
        pg_phy_addr = addr_v2p(vaddr);
        pfree(pg_phy_addr);
        page_table_pte_remove(vaddr);
        ++page_cnt;
        vaddr += PG_SIZE;    
    }
    vaddr_remove(pf, _vaddr, pg_cnt);
}

void sys_free(void* ptr) {
    ASSERT(ptr != NULL);
    enum pool_flags PF;
    struct pool* mem_pool;
    if (running_thread()->pgdir == NULL) {
        ASSERT((uint32_t) ptr > K_HEAP_START);
        PF = PF_KERNEL;
        mem_pool = &kernel_pool;
    } else {
        PF = PF_USER;
        mem_pool = &user_pool;
    }

    lock_acquire(&mem_pool->lock);
    struct mem_block* b = ptr;
    struct arena* a = block2arena(b);
    if (a->desc == NULL && a->large == true) {
        mfree_page(PF, a, a->cnt);
    } else {
        list_append(&a->desc->free_list, &b->free_elem);
        ++a->cnt;
        if(a->cnt == a->desc->blocks_per_arena) {
            uint32_t block_idx;
            for(block_idx = 0; block_idx < a->desc->blocks_per_arena; ++block_idx) {
                b = arena2block(a, block_idx);
                ASSERT(elem_find(&a->desc->free_list, &b->free_elem));
                list_remove(&b->free_elem);
            }
            mfree_page(PF, a, 1);
        }
    }
    lock_release(&mem_pool->lock);
}
