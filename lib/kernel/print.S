RPL0    equ 00b
TI_GDT  equ 000b
SELECTOR_VIDEO       equ (0X0003<<3) + TI_GDT + RPL0 
SELECTOR_DATA	  equ (0X0002<<3) + TI_GDT + RPL0
section .data
put_int_buffer dq 0

[bits 32]
section .text

global put_str
;------------------- put_char 函数实现 -------------------------------------------
;把字符写到光标位置
;---------------------------------------------------------------------------------
put_str:
   push ebx
   push ecx
   xor ecx,ecx
   mov ebx,[esp+12]
.goon:
   mov cl,[ebx]
   cmp cl,0
   jz .str_over
   push ecx
   call put_char
   add esp,4
   inc ebx
   jmp .goon
.str_over:
   pop ecx
   pop ebx
   ret

global put_char
put_char:

    pushad           		;push all double寄存器
    mov ax,SELECTOR_VIDEO
    mov gs,ax                  ;gs寄存器赋值段选择子
    
    mov dx,0x3D4               ;默认CRT 寄存器索引
    mov al,0xE                 ;这里用al 不用ax 是因为 此处索引寄存器是一个8字节的寄存器
    out dx,al                  ;看光标高位值寄存器 为什么用al 可以推算一下 光标0~1999 2^(8+8)字节完全够用
    mov dx,0x3D5               ;光标高位值寄存器窗口
    in  al,dx                  ;移入ax 
    mov ah,al                 ;左移动8位 移动向ah部分 光标位置为8字节
    
    mov dx,0x3D4
    mov al,0xF
    out dx,al
    mov dx,0x3D5
    in  al,dx
    
    mov bx,ax                  ;光标位置转移给bx  
    mov ecx,[esp+36]       ;4字节返回 只需要把push eip 4字节 + pushad 8*4 32字节算进去即可
                               
                               ;回车0xd 换行0xa 退格0x8 
    cmp cl,0xd                 ;比较cl与0xd是否相等
    jz .is_carriage_return     ;回车处理函数
    cmp cl,0xa                 ;比较cl与0xa是否相等 
    jz .is_line_feed           ;换行处理函数
    cmp cl,0x8                 ;比较cl与0x8是否相等
    jz .is_backspace           ;退格处理函数
    
    jmp .put_other_char
       
 .is_backspace:
    cmp bx,0                   ;如果bx = 0 则没有办法退格了 这是我除书上之外额外加的条件
    je  .set_cursor              
    
    dec bx                     ;光标位置退1
    shl bx,1                   ;一个字符占两个字节 一个字节是属性 一个字节ascii 
    
    mov word [gs:bx],0x0720    ;低字节ascii 32 ascii表示空字符 7 高字符属性空
    shr bx,1                   ;退回原来的位置
    jmp .set_cursor            ;交给硬件去处理光标位置 到时候在那几个老端口把字符信息还回去就完事了
    
 .put_other_char:
    shl bx,1                   
    mov [gs:bx],cl             
    inc bx                     
    mov byte [gs:bx],0x7
    inc bx
    shr bx,1
    cmp bx,2000                ;没到边界2000即跳转
    jl  .set_cursor            ;没有的话 继续往下进行 都2000了其实下面的操作也是不能跳转走的
                               ;只能等下面继续处理滚动屏幕了
                               
    
 
 .is_line_feed:
 .is_carriage_return:
    xor dx,dx
    mov ax,bx                  ;32位除以16位 被除数高位dx 被除数低位ax div之后余数放在dx 商放在ax
    mov si,80
    div si
    sub bx,dx
    
 .is_carrige_return_end:
    add bx,80                  ;一共一页2000 共25行 2000/25=80 则向bx增加80
    cmp bx,2000
 .is_line_feed_end:
    jl .set_cursor             
    
 .roll_screen:
    cld                        ;从低到高移动                 
    mov ecx,960
    
    mov esi,0xc00b80a0          ;忘了的这里写一下 ds:si -> es:di (s->d) 源地址si 目的地址di  
    mov edi,0xc00b8000
    rep movsd                  ;movs doubleword 双子
    
    mov ebx,3840               ;最后一行80*2 = 160 4000-160 = 3840 最后一行清除了
    mov ecx,80                
    
 .clean_last_row:   
    mov word [gs:ebx],0x720 ;0x07 0x20 属性 空字符
    add ebx,2   
    loop .clean_last_row
    
    mov ebx,1920
    
 .set_cursor:
    mov dx,0x3D4
    mov al,0xE
    out dx,al
    mov dx,0X3D5
    mov al,bh
    out dx,al
    
    mov dx,0x3D4
    mov al,0XF
    out dx,al
    mov dx,0x3D5
    mov al,bl
    out dx,al
.put_char_done:
    popad                        ;把之前全部储存的给pop出来 还原现场
    ret

global put_int
put_int:
   pushad
   mov ebp,esp
   mov eax,[ebp+4*9]
   mov edx,eax
   mov edi,7 ;缓冲区中初始偏移量
   mov ecx,8 ;填充8个16进制位
   mov ebx,put_int_buffer

.16based_bits:
   and edx,0x0000000f ;解析每一位
   cmp edx,9
   jg .isA2F
   add edx,'0'
   jmp .store
.isA2F:
   sub edx,10
   add edx,'A'
.store:
   mov [ebx+edi],dl
   dec edi
   shr eax,4
   mov edx,eax
   loop .16based_bits
.ready_to_print:
   inc edi
.skip_prefix_0:
   cmp edi,8
   je .full0
.go_on_skip:
   mov cl,[put_int_buffer+edi]
   inc edi
   cmp cl,'0'
   je .skip_prefix_0
   dec edi
   jmp .put_each_num
.full0:
   mov cl,'0'
.put_each_num:
   push ecx
   call put_char
   add esp,4
   inc edi
   mov cl,[put_int_buffer+edi]
   cmp edi,8
   jl .put_each_num
   popad
   ret