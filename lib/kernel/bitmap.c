#include "bitmap.h"
#include "stdint.h"
#include "string.h"
#include "interrupt.h"         
#include "print.h"             
#include "debug.h"

void bitmap_init(struct bitmap* btmp){
    memset(btmp->bits,0,btmp->btmp_bytes_len);
    return;
}
bool bitmap_scan_test(struct bitmap* btmp,uint32_t bit_idx){
    int index=bit_idx/8;
    int pos=bit_idx%8;
    return (btmp->bits[index])&(BITMAP_MASK<<pos);
}
int bitmap_scan(struct bitmap* btmp,uint32_t cnt){
    int l=0,r=0;
    //int index=0,pos=0;//index记录字节下标，pos记录偏移量
    int zeronum=0;
    for(r=l;r<cnt;++r){
        if(!bitmap_scan_test(btmp,r)){
            ++zeronum;
        }
    }
    while(r<btmp->btmp_bytes_len){
        if(zeronum==cnt){
            return l;
        }
        if(!bitmap_scan_test(btmp,l++)) --zeronum;
        if(!bitmap_scan_test(btmp,r++)) ++zeronum;
    }
    return -1;
}
void bitmap_set(struct bitmap* btmp,uint32_t bit_idx,int8_t value){
    ASSERT((value==0)||(value==1));
    int index=bit_idx/8;
    int pos=bit_idx%8;
    if(value){
        btmp->bits[index]|=(BITMAP_MASK<<pos);
    }else{
        btmp->bits[index]&=~(BITMAP_MASK<<pos);
    }
}