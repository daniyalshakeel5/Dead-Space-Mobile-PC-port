#include <assert.h>
#include <stdio.h>
#include "texture_ktx.h"
static void put(unsigned char* p,unsigned v){p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}
int main(void){
 unsigned char b[84]={0xab,0x4b,0x54,0x58,0x20,0x31,0x31,0xbb,13,10,26,10};DsKtx k;
 put(b+12,0x04030201);put(b+20,1);put(b+28,0x8d64);put(b+32,0x1907);
 put(b+36,4);put(b+40,4);put(b+52,1);put(b+56,1);put(b+64,8);
 assert(ds_ktx_parse(b,76,&k)&&k.width==4&&k.size[0]==8);
 for(size_t n=0;n<76;n++)assert(!ds_ktx_parse(b,n,&k));
 put(b+64,9);assert(!ds_ktx_parse(b,76,&k));put(b+64,8);
 put(b+36,8193);assert(!ds_ktx_parse(b,76,&k));put(b+36,4);
 put(b+48,1);assert(!ds_ktx_parse(b,76,&k));put(b+48,0);
 put(b+56,15);assert(!ds_ktx_parse(b,76,&k));put(b+56,1);
 put(b+28,0x87ee);put(b+64,16);assert(ds_ktx_parse(b,84,&k));
 unsigned char atc[16]={255,0},out[64];atc[8]=0;atc[9]=0x7c;
 ds_atc_block(atc,out,0x87ee);
 for(int i=0;i<16;i++)assert(out[i*4]==255&&out[i*4+1]==0&&out[i*4+2]==0&&out[i*4+3]==255);
 memset(atc,0,sizeof atc);memset(atc,0x55,8);atc[8]=0;atc[9]=0x7c;
 ds_atc_block(atc,out,0x8c93);
 for(int i=0;i<16;i++)assert(out[i*4]==255&&out[i*4+3]==85);
 /* Interpolated-alpha selector 7 chooses 1/7 alpha0 when alpha0>alpha1. */
 memset(atc,0,sizeof atc);atc[0]=255;atc[1]=0;memset(atc+2,0xff,6);atc[9]=0x7c;
 ds_atc_block(atc,out,0x87ee);for(int i=0;i<16;i++)assert(out[i*4+3]==36);
 puts("PASS KTX bounds, dimensions, mip data and ATC color/alpha");return 0;
}
