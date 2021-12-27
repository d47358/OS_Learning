%include "boot.inc"
SECTION MBR vstart=0x7c00 ;起始地质编译在0x7c00
    mov ax,cs
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov fs,ax
    mov sp,0x7c00
    mov ax,0xb800 ; ax为文本信号起始区
    mov gs,ax     ; gs = ax 充当段基址的作用

    ;ah = 0x06 al = 0x00 想要调用int 0x06的BIOS提供的中断对应的函数 即向上移动即完成清屏功能
    ;cx dx 分别存储左上角与右下角的左边 详情看int 0x06函数调用
    mov ax,0600h 
    mov bx,0700h
    mov cx,0
    mov dx,184fh
    
    ;调用BIOS中断
    int 10h 
    
    mov byte [gs:0x00],'1' ;低位字节储存ascii字符 小端储存内存顺序相反
    mov byte [gs:0x01],0xA4    ;背景储存在第二个字节 含字符与背景属性
    
    mov byte [gs:0x02],' ' 
    mov byte [gs:0x03],0xA4
    
    mov byte [gs:0x04],'M' 
    mov byte [gs:0x05],0xA4
    
    mov byte [gs:0x06],'B' 
    mov byte [gs:0x07],0xA4
    
    mov byte [gs:0x08],'R' 
    mov byte [gs:0x09],0xA4

    mov eax,LOADER_START_SECTOR ;起始扇区LBA地址
    mov bx,LOADER_BASE_ADDR ;写入的地址
    mov cx,4 ;读入扇区数
    call rd_disk_m16

    jmp LOADER_BASE_ADDR
    
;读取n个扇区
rd_disk_m16:
    ;设置读取的扇区数
    mov esi,eax
    mov di,cx
    mov dx,0x1f2
    mov al,cl
    out dx,al
    ;恢复eax
    mov eax,esi

;设置LBA地址

    ;0-7位
    mov dx,0x1f3
    out dx,al;
    ;8-15位
    mov cl,8
    shr eax,cl
    mov dx,0x1f4
    out dx,al
    ;16-23位
    shr eax,cl
    mov dx,0x1f5
    out dx,al

    ;余下3位放入device寄存器
    shr eax,cl
    and eax,0x0f ;保留低4位
    or eax,0xe0 ;设置寻址模式，主盘从盘
    mov dx,0x1f6 ;device端口
    out dx,al

    ;command端口
    mov dx,0x1f7
    mov al,0x20 ;读操作
    out dx,al

    ;检测硬盘状态
  .not_ready:
    nop
    in al,dx
    and al,0x88
    cmp al,0x08
    jnz .not_ready

    ;读数据
    mov ax,di
    mov dx,256
    mul dx
    mov cx,ax
    mov dx,0x1f0

  .go_on_read:
    in ax,dx
    mov[bx],ax
    add bx,2
    loop .go_on_read ;cx存储循环次数
    ret

    
  times 510 - ($ - $$) db 0 
  db 0x55,0xaa
