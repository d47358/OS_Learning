#include "ide.h"
#include "sync.h"
#include "stdio.h"
#include "stdio-kernel.h"
#include "interrupt.h"
#include "memory.h"
#include "debug.h"
#include "string.h"
#include "timer.h"

/* 定义硬盘各寄存器的端口号 */
#define reg_data(channel)	 (channel->port_base + 0)
#define reg_error(channel)	 (channel->port_base + 1)
#define reg_sect_cnt(channel)	 (channel->port_base + 2)
#define reg_lba_l(channel)	 (channel->port_base + 3)
#define reg_lba_m(channel)	 (channel->port_base + 4)
#define reg_lba_h(channel)	 (channel->port_base + 5)
#define reg_dev(channel)	 (channel->port_base + 6)
#define reg_status(channel)	 (channel->port_base + 7)
#define reg_cmd(channel)	 (reg_status(channel))
#define reg_alt_status(channel)  (channel->port_base + 0x206)
#define reg_ctl(channel)	 reg_alt_status(channel)

/* reg_alt_status寄存器的一些关键位 */
#define BIT_STAT_BSY	 0x80	      // 硬盘忙
#define BIT_STAT_DRDY	 0x40	      // 驱动器准备好	 
#define BIT_STAT_DRQ	 0x8	      // 数据传输准备好了

/* device寄存器的一些关键位 */
#define BIT_DEV_MBS	0xa0	    // 第7位和第5位固定为1 0x10100000
#define BIT_DEV_LBA	0x40
#define BIT_DEV_DEV	0x10

/* 一些硬盘操作的指令 */
#define CMD_IDENTIFY	   0xec	    // identify指令
#define CMD_READ_SECTOR	   0x20     // 读扇区指令
#define CMD_WRITE_SECTOR   0x30	    // 写扇区指令

/* 定义可读写的最大扇区数,调试用的 */
#define max_lba ((80*1024*1024/512) - 1)	// 只支持80MB硬盘

uint8_t channel_cnt;	   // 按硬盘数计算的通道数
struct ide_channel channels[2];	 // 有两个ide通道

/* 硬盘数据结构初始化 */
void ide_init() {
   printk("ide_init start\n");
   uint8_t hd_cnt = *((uint8_t*)(0x475));	      // 获取硬盘的数量
   ASSERT(hd_cnt > 0);
   channel_cnt = DIV_ROUND_UP(hd_cnt, 2);	   // 一个ide通道上有两个硬盘,根据硬盘数量反推有几个ide通道
   struct ide_channel* channel;
   uint8_t channel_no = 0;

   /* 处理每个通道上的硬盘 */
   while (channel_no < channel_cnt) {
      channel = &channels[channel_no];
      sprintf(channel->name, "ide%d", channel_no);

      /* 为每个ide通道初始化端口基址及中断向量 */
      switch (channel_no) {
	 case 0:
	    channel->port_base	 = 0x1f0;	   // ide0通道的起始端口号是0x1f0
	    channel->irq_no	 = 0x20 + 14;	   // 从片8259a上倒数第二的中断引脚,温盘,也就是ide0通道的的中断向量号
	    break;
	 case 1:
	    channel->port_base	 = 0x170;	   // ide1通道的起始端口号是0x170
	    channel->irq_no	 = 0x20 + 15;	   // 从8259A上的最后一个中断引脚,我们用来响应ide1通道上的硬盘中断
	    break;
      }

      channel->expecting_intr = false;		   // 未向硬盘写入指令时不期待硬盘的中断
      lock_init(&channel->lock);		     

   /* 初始化为0,目的是向硬盘控制器请求数据后,硬盘驱动sema_down此信号量会阻塞线程,
   直到硬盘完成后通过发中断,由中断处理程序将此信号量sema_up,唤醒线程. */
      sema_init(&channel->disk_done, 0);
      channel_no++;				   // 下一个channel
   }
   printk("ide_init done\n");
}

static void select_disk(struct disk* hd) {
    uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA;
    if (hd->dev_no == 1)
        reg_device |= BIT_DEV_DEV;
    outb(reg_dev(hd->my_channel), reg_device);
}

static void select_sector(struct disk* hd, uint32_t lba, uint8_t dec_cnt) {
    ASSERT(lba <= max_lba);
    struct ide_channel* channel = hd->my_channel;
    //写入要操作的扇区数
    outb(reg_sect_cnt(channel), dec_cnt);
    //写入lba地址
    outb(reg_lba_l(channel), lba);
    outb(reg_lba_m(channel), lba >> 8);
    outb(reg_lba_h(channel), lba >> 16);
    //lba地址0-3位单独写入
    outb(reg_dev(channel), BIT_DEV_MBS | BIT_DEV_LBA | (hd->dev_no == 1 ? BIT_DEV_DEV : 0) | lba >>24);
}

static void cmd_out(struct ide_channel* channel, uint8_t cmd) {
    channel->expecting_intr = true;
    outb(reg_cmd(channel), cmd);
}

static void read_from_sector(struct disk* hd, void* buf, uint8_t sec_cnt) {
    uint32_t size_in_bytes;
    if (sec_cnt == 0)
        size_in_bytes = 256 * 512;
    else
        size_in_bytes = sec_cnt * 512;
    insw(reg_data(hd->my_channel), size_in_bytes / 2);
}

static void write2sector(struct disk* hd, void* buf, uint8_t sec_cnt) {
    uint32_t size_in_bytes;
    if (sec_cnt == 0)
        size_in_bytes = 256 * 512;
    else
        size_in_bytes = sec_cnt * 512;
    outsw(reg_data(hd->my_channel), buf, size_in_bytes / 2);
}

static bool busy_wait(struct disk* hd) {
    struct ide_channel* channel = hd->my_channel;
    uint16_t time_limit = 30 * 1000;
    while (time_limit -= 10 >= 0) {
        if(!inb(reg_status(channel), BIT_STAT_BSY)) {
            return (inb(reg_status(channel)) & BIT_STAT_DRQ);
        }else
            mtime_sleep(10);
    }
}

//从硬盘读取sec_cnt扇区到buf
void ide_read(struct disk* hd,uint32_t lba,void* buf,uint32_t sec_cnt)
{
    ASSERT(lba <= max_lba);
    ASSERT(sec_cnt > 0);
    lock_acquire(&hd->my_channel->lock);
    
    select_disk(hd);
    
    uint32_t secs_op;
    uint32_t secs_done = 0;
    while(secs_done < sec_cnt)
    {
        if((secs_done + 256) <= sec_cnt)    secs_op = 256;
        else	secs_op = sec_cnt - secs_done;
    	
        select_sector(hd,lba + secs_done, secs_op);
        cmd_out(hd->my_channel,CMD_READ_SECTOR); //执行命令
        
        /*在硬盘开始工作时 阻塞自己 完成读操作后唤醒自己*/
        sema_down(&hd->my_channel->disk_done);
        
        /*检测是否可读*/
        if(!busy_wait(hd))
        {
            char error[64];
            sprintf(error,"%s read sector %d failed!!!!\n",hd->name,lba);
            PANIC(error);
        }
        
      	read_from_sector(hd,(void*)((uint32_t)buf +secs_done * 512),secs_op);
      	secs_done += secs_op;
    }
    lock_release(&hd->my_channel->lock);   
}

void ide_write(struct disk* hd,uint32_t lba,void* buf,uint32_t sec_cnt)
{
    ASSERT(lba <= max_lba);
    ASSERT(sec_cnt > 0);
    lock_acquire(&hd->my_channel->lock);
    
    select_disk(hd);
    uint32_t secs_op;
    uint32_t secs_done = 0;
    while(secs_done < sec_cnt)
    {
        if((secs_done + 256) <= sec_cnt)    secs_op = 256;
        else	secs_op = sec_cnt - secs_done;
        
    	select_sector(hd,lba+secs_done,secs_op);
    	cmd_out(hd->my_channel,CMD_WRITE_SECTOR);
    	
    	if(!busy_wait(hd))
    	{
    	    char error[64];
    	    sprintf(error,"%s write sector %d failed!!!!!!\n",hd->name,lba);
    	    PANIC(error);
    	}
    	
    	write2sector(hd,(void*)((uint32_t)buf + secs_done * 512),secs_op);
    	
    	//硬盘响应期间阻塞
    	sema_down(&hd->my_channel->disk_done);
    	secs_done += secs_op;
    }
    lock_release(&hd->my_channel->lock);
}

//硬盘结束任务中断程序
void intr_hd_handler(uint8_t irq_no)
{
    ASSERT(irq_no == 0x2e || irq_no == 0x2f);
    uint8_t ch_no = irq_no - 0x20 - 0xe;
    struct ide_channel* channel = &channels[ch_no];
    ASSERT(channel->irq_no == irq_no);
    if(channel->expecting_intr)
    {
	channel->expecting_intr = false;//结束任务了
	sema_up(&channel->disk_done);
	inb(reg_status(channel));
    }   
}
