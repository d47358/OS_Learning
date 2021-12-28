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

    jmp $