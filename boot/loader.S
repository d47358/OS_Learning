%include "boot.inc"
SECTION loader vstart=LOADER_BASE_ADDR
LOADER_STACK_TOP equ LOADER_BASE_ADDR
;jmp loader_start

GDT_BASE: dd 0x00000000
          dd 0x00000000 ;第0个描述符不可用
CODE_DESC: dd 0x0000FFFF
           dd DESC_CODE_HIGH4
DATA_STACK_DESC: dd 0x0000FFFF
                 dd DESC_DATA_HIGH4           
VIDEO_DESC: dd 0x80000007
            dd DESC_VIDEO_HIGH4

GDT_SIZE equ $-GDT_BASE
GDT_LIMIE equ GDT_SIZE-1

times 60 dq 0

total_mem_bytes dd 0 ;60个四字+4个描述符=512字节=0x200，loader加载地址为0x900,此处地址为0x900+0x200=0xb00

SELECTOR_CODE equ (0x0001<<3)+TI_GDT+RPL_0
SELECTOR_DATA equ (0x0002<<3)+TI_GDT+RPL_0
SELECTOR_VIDEO equ (0x0003<<3)+TI_GDT+RPL_0

gdt_ptr dw GDT_LIMIE
        dd GDT_BASE
ards_buf times 244 db 0
ards_nr dw 0

; total_mem_bytes偏移量为0x200，dd+dw+dd+240*db+dw=256字节=0x100,此处偏移为0x300
loader_start:
    ;0xe820获取内存
    xor ebx,ebx
    mov edx,0x534d4150
    mov di,ards_buf

    .e820_get_loop:
    mov eax,0xe820
    mov ecx,20
    int 0x15
    jc .e820_failed_try_e801
    add di,cx
    inc word [ards_nr]
    cmp ebx,0
    jnz .e820_get_loop ;获取所有ards结构体
    mov cx,[ards_nr]
    mov ebx,ards_buf
    xor edx,edx
    .find_max_mem: ;找出最大的内存
    mov eax,[ebx]
    add eax,[ebx+8]
    add ebx,20
    cmp edx,eax
    jge .next_ards
    mov edx,eax
    .next_ards:
        loop .find_max_mem
        jmp .mem_get_ok

    .e820_failed_try_e801:
    mov ax,0xe801
    int 0x15
    jc .e801_failed_try_88
    ;低15M内存
    mov cx,0x400
    mul cx  ;换算为字节，乘1024，高16位保存在dx
    shl edx,16
    and eax,0x0000FFFF
    or edx,eax
    add edx,0x100000 ;加上1M
    mov esi,edx
    ;16M以上
    xor eax,eax
    mov ax,bx
    mov ecx,0x10000
    mul ecx ;以64KB为单位 64*1024=0x10000
    add esi,eax
    mov edx,esi
    jmp .mem_get_ok

    .e801_failed_try_88:
    mov ah,0x55
    int 0x15
    jc .error_hlt
    and eax,0x0000FFFF
    mov cx,0x400
    mul cx ;以1K为单位
    shl edx,16
    or edx,eax
    add edx,0x100000 ;加上1M

    .error_hlt:
     jmp $

    .mem_get_ok:
        mov [total_mem_bytes],edx



    ;进入保护模式
    ;打开A20
    in al,0x92
    or al,0000_0010b
    out 0x92,al

    ;加载GDT
    lgdt [gdt_ptr]

    ;设置cr0寄存器
    mov eax,cr0
    or eax,0x00000001
    mov cr0,eax
    jmp dword SELECTOR_CODE:p_mode_start

[bits 32]
p_mode_start:
    mov ax,SELECTOR_DATA
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov esp,LOADER_STACK_TOP
    mov ax,SELECTOR_VIDEO
    mov gs,ax
    mov byte [gs:160],'P'

;加载kernel
mov eax,KERNEL_START_SECTOR
mov ebx,KERNEL_BIN_BASE_ADDR
mov ecx,200 ;读入扇区数
call rd_disk_m32


call setup_page
sgdt[gdt_ptr] ;导出描述符地址与偏移量
;0xc0000000---0xffffffff为3-4GB
mov ebx,[gdt_ptr+2] ;gdt描述符低两字节是偏移量
or dword[ebx+0x18+4],0xc0000000 ;显存段描述符是第三个，因此加上3*8=24字节，再加上段描述符低四字节，将高四字节的段基址修改到3GB以上
add dword[gdt_ptr+2],0xc0000000 ;gdt基址加上3GB
add esp,0xc0000000 ;栈基址加上3GB
;页目录地址赋给cr3
mov eax,PAGE_DIR_TABLE_POS
mov cr3,eax
;打开cr0的pg位
mov eax,cr0
or eax,0x80000000
mov cr0,eax
;重新加载gdt
lgdt[gdt_ptr]
mov byte [gs:160],'V'
;jmp $
jmp SELECTOR_CODE:enter_kernel
enter_kernel:
    call kernel_init
    mov esp,0xc009f000
    jmp KERNEL_ENTRY_POINT

jmp $
;开启分页
;清空页目录
setup_page:
    mov ecx,4096
    mov esi,0
.clear_page_dir:
    mov byte[PAGE_DIR_TABLE_POS+esi],0
    inc esi
    loop .clear_page_dir
;创建页目录项
.create_pde:
    mov eax,PAGE_DIR_TABLE_POS
    add eax,0x1000
    mov ebx,eax 
    or eax,PG_P|PG_RW_W|PG_US_U
    mov [PAGE_DIR_TABLE_POS],eax
    mov [PAGE_DIR_TABLE_POS+0xc00],eax ;第0个和第768个目录项映射为第0个页表项
    sub eax,0x1000
    mov [PAGE_DIR_TABLE_POS+4092],eax ;最后一个页目录项指向页目录表位置
;创建页表项
    mov ecx,256
    mov esi,0
    xor edx,edx
    or edx,PG_P|PG_RW_W|PG_US_U
.create_pte:
    mov [ebx+esi*4],edx
    add edx,4096
    inc esi
    loop .create_pte

;创建内核其他页目录项
    mov eax,PAGE_DIR_TABLE_POS
    add eax,0x2000
    or eax,PG_P|PG_RW_W|PG_US_U
    mov ebx,PAGE_DIR_TABLE_POS
    mov ecx,254 ;内核256个页目录项，去掉第一个和最后一个
    mov esi,769
.create_kernel_pde:
    mov [ebx+esi*4],eax
    inc esi
    add eax,0x1000
    loop .create_kernel_pde
    ret

rd_disk_m32:
    ;备份eax，ecx
    mov esi,eax
    mov di,cx
    ;设置读取的扇区数
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
    mov[ebx],ax
    add ebx,2
    loop .go_on_read ;cx存储循环次数
    ret


kernel_init:
    xor eax,eax
    xor ebx,ebx
    xor ecx,ecx
    xor edx,edx

    mov dx,[KERNEL_BIN_BASE_ADDR+42] ;文件偏移42字节处是program header大小
    mov ebx,[KERNEL_BIN_BASE_ADDR+28] ; 偏移28字节处是第一个program header的偏移量
    add ebx,KERNEL_BIN_BASE_ADDR ;得到程序头表物理地址
    mov cx,[KERNEL_BIN_BASE_ADDR+44] ;程序头数量
    ;读取每个段
.each_segment:
    cmp byte [ebx],PT_NULL ;是否为空
    je .PTNULL
    ;为memcpy压入参数,memcpy(dst,src,size)
    push dword [ebx+16] ;程序头偏移16字节处是p_filesz，压入size
    mov eax,[ebx+4] ;程序头偏移4字节处是p_offset
    add eax,KERNEL_BIN_BASE_ADDR 
    push eax
    push dword [ebx+8] ;偏移8字节是p_vaddr
    call mem_cpy
    add esp,12 ;清除栈中三个参数
.PTNULL:
    add ebx,edx ;edx为program header大小，使eax指向下一个程序头
    loop .each_segment
    ret
mem_cpy:
    cld ;使用movs时，自动使si,di加上移动的字节数 std为减去
    push ebp ;备份栈基址
    mov ebp,esp 
    push ecx
    mov edi,[ebp+8] ;dst +4是mem_cpy返回地址
    mov esi,[ebp+12] ;src
    mov ecx,[ebp+16] ;size
    rep movsb ;将si逐字节拷贝至di

    pop ecx
    pop ebp
    ret




