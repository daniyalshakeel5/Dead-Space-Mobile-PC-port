/* KTX1 validation and ATC decoding for user-supplied Reanimated textures.
 * ATC color interpolation adapted from AssetStudio's MIT-licensed decoder;
 * see THIRD_PARTY_ASSETSTUDIO_LICENSE.txt. All output is RGBA byte order. */
#ifndef DS_TEXTURE_KTX_H
#define DS_TEXTURE_KTX_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef struct { unsigned width,height,format,levels; const unsigned char* data[14]; size_t size[14]; } DsKtx;
static uint32_t ds_le32(const unsigned char* p) { return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static int ds_ktx_parse(const unsigned char* b,size_t n,DsKtx* k) {
    static const unsigned char magic[12]={0xab,0x4b,0x54,0x58,0x20,0x31,0x31,0xbb,13,10,26,10};
    if(n<64 || memcmp(b,magic,12) || ds_le32(b+12)!=0x04030201 || ds_le32(b+16) || ds_le32(b+20)!=1 || ds_le32(b+24) || ds_le32(b+44) || ds_le32(b+48) || ds_le32(b+52)!=1) return 0;
    memset(k,0,sizeof *k); k->format=ds_le32(b+28); k->width=ds_le32(b+36); k->height=ds_le32(b+40); k->levels=ds_le32(b+56);
    if(!k->width || !k->height || k->width>8192 || k->height>8192 || !k->levels || k->levels>14) return 0;
    unsigned block=k->format==0x8d64 || k->format==0x8c92 ? 8 : k->format==0x8c93 || k->format==0x87ee ? 16 : 0;
    if(!block || ds_le32(b+60)>n-64) return 0;
    size_t offset=64+(size_t)ds_le32(b+60); unsigned w=k->width,h=k->height;
    for(unsigned l=0;l<k->levels;l++) {
        if(offset>n || n-offset<4) return 0;
        size_t size=ds_le32(b+offset); offset+=4;
        size_t expected=(size_t)((w+3)/4)*((h+3)/4)*block;
        if(size!=expected || size>n-offset) return 0;
        k->data[l]=b+offset; k->size[l]=size;
        size_t padded=(size+3)&~(size_t)3;
        if(padded>n-offset) return 0;
        offset+=padded;
        if(w==1 && h==1 && l+1<k->levels) return 0;
        w=w>1?w/2:1; h=h>1?h/2:1;
    }
    return offset==n;
}
static uint64_t ds_texture_hash(unsigned w,unsigned h,unsigned format,const unsigned char* p,size_t n) {
    uint64_t hash=UINT64_C(0xcbf29ce484222325);
    unsigned v[3]={w,h,format};
    for(int j=0;j<3;j++) for(int i=0;i<4;i++) { hash^=(v[j]>>(8*i))&255; hash*=UINT64_C(0x100000001b3); }
    for(size_t i=0;i<n;i++) { hash^=p[i]; hash*=UINT64_C(0x100000001b3); }
    return hash;
}
static int ds_expand(unsigned v,unsigned bits) { v<<=8-bits; return v|(v>>bits); }
static void ds_atc_block(const unsigned char* src,unsigned char rgba[64],unsigned format) {
    const unsigned char* c=src+(format==0x8c92?0:8);
    unsigned a=c[0]|c[1]<<8,b=c[2]|c[3]<<8;
    int colors[4][3]={{0}};
    int first=(a&0x8000)?2:0;
    colors[first][0]=ds_expand((a>>10)&31,5); colors[first][1]=ds_expand((a>>5)&31,5); colors[first][2]=ds_expand(a&31,5);
    colors[3][0]=ds_expand((b>>11)&31,5); colors[3][1]=ds_expand((b>>5)&63,6); colors[3][2]=ds_expand(b&31,5);
    for(int ch=0;ch<3;ch++) {
        if(first==0) { colors[1][ch]=(5*colors[0][ch]+3*colors[3][ch])/8; colors[2][ch]=(3*colors[0][ch]+5*colors[3][ch])/8; }
        else { int d=colors[2][ch]-colors[3][ch]/4; colors[1][ch]=d>0?d:0; }
    }
    unsigned indices=ds_le32(c+4); unsigned char alpha[8]={src[0],src[1]}; uint64_t bits=0;
    if(format==0x87ee) {
        if(alpha[0]>alpha[1]) for(int j=2;j<8;j++) alpha[j]=((8-j)*alpha[0]+(j-1)*alpha[1])/7;
        else { for(int j=2;j<6;j++) alpha[j]=((6-j)*alpha[0]+(j-1)*alpha[1])/5; alpha[6]=0;alpha[7]=255; }
        for(int j=0;j<6;j++)bits|=(uint64_t)src[j+2]<<(8*j);
    }
    for(int i=0;i<16;i++) {
        unsigned idx=(indices>>(2*i))&3;
        for(int j=0;j<3;j++)rgba[i*4+j]=(unsigned char)colors[idx][j];
        rgba[i*4+3]=format==0x8c93 ? ((src[i/2]>>((i%2)*4))&15)*17 : format==0x87ee ? alpha[(bits>>(3*i))&7] : 255;
    }
}
#endif
