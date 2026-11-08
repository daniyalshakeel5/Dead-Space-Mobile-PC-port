/* Optional content-addressed HD replacement at the GLES upload boundary. */
#include "texture_ktx.h"
typedef struct DsHdTexture {
    u32 id,w,h,internal,format,type,compressed; size_t bytes;
    int replaced,dynamic; u8* original; struct DsHdTexture* next;
} DsHdTexture;
static DsHdTexture* ds_hd_textures;
static DsHdTexture* ds_hd_bound(void) {
    s32 id=0; if(!dgl_glGetIntegerv)return NULL;
    dgl_glGetIntegerv(0x8069,&id); /* current unit's GL_TEXTURE_BINDING_2D */
    for(DsHdTexture* t=ds_hd_textures;t;t=t->next) if(t->id==(u32)id)return t;
    return NULL;
}
static void ds_hd_delete(s32 n,const u32* ids) {
    if(n<0 || !ids)return;
    for(s32 i=0;i<n;i++) {
        DsHdTexture** p=&ds_hd_textures;
        while(*p) { DsHdTexture* t=*p; if(t->id==ids[i]) { *p=t->next;free(t->original);free(t);break; } p=&t->next; }
    }
}
static u8* ds_hd_decode(const u8* data,u32 w,u32 h,u32 fmt) {
    u8* rgba=malloc((size_t)w*h*4); if(!rgba)return NULL;
    u32 bw=(w+3)/4,bh=(h+3)/4,bs=fmt==0x8d64||fmt==0x8c92?8:16;
    for(u32 by=0;by<bh;by++)for(u32 bx=0;bx<bw;bx++) {
        u8 block[64];const u8* src=data+((size_t)by*bw+bx)*bs;
        if(fmt==0x8d64) { u8 rgb[48];etc1_block(src,rgb); for(int i=0;i<16;i++){memcpy(block+4*i,rgb+3*i,3);block[4*i+3]=255;} }
        else ds_atc_block(src,block,fmt);
        for(u32 y=0;y<4 && by*4+y<h;y++)for(u32 x=0;x<4 && bx*4+x<w;x++)
            memcpy(rgba+((size_t)(by*4+y)*w+bx*4+x)*4,block+(y*4+x)*4,4);
    }
    return rgba;
}
static void ds_hd_restore(DsHdTexture* t) {
    if(!t || !t->replaced)return;
    dgl_glPixelStorei(0x0cf5,1);
    if(t->compressed) {
        u8* rgb=etc1_decode(t->original,t->w,t->h);
        if(rgb){dgl_glTexImage2D(0xde1,0,0x1907,t->w,t->h,0,0x1907,0x1401,rgb);free(rgb);}
    } else dgl_glTexImage2D(0xde1,0,t->internal,t->w,t->h,0,t->format,t->type,t->original);
    dgl_glPixelStorei(0x0cf5,g_unpack_align);
    if(dgl_glTexParameteri)dgl_glTexParameteri(0xde1,0x813d,1000);
    if(dgl_glGenerateMipmapEXT)dgl_glGenerateMipmapEXT(0xde1);
    t->replaced=0;free(t->original);t->original=NULL;
}
static void ds_hd_subimage(u32 target) {
    if(target!=0xde1)return;
    DsHdTexture* t=ds_hd_bound();
    if(t){ds_hd_restore(t);t->dynamic=1;}
}
static int ds_hd_image(u32 target,s32 level,u32 internal,u32 w,u32 h,u32 border,u32 format,u32 type,const u8* data,size_t size,int compressed) {
    const char* root=getenv("DS_HD_TEXTURES");
    if(!root || !*root || !strcmp(root,"0") || target!=0xde1 || border)return 0;
    DsHdTexture* t=ds_hd_bound();
    if(level!=0)return level>0 && t && t->replaced;
    if(t){t->replaced=0;free(t->original);t->original=NULL;if(t->dynamic)return 0;}
    if(!data || !w || !h || w>8192 || h>8192)return 0;
    size_t row=0;
    if(compressed) {
        if(format!=0x8d64 || size!=(size_t)((w+3)/4)*((h+3)/4)*8)return 0;
    } else {
        unsigned components=format==0x1908?4:format==0x1907?3:format==0x190a?2:format==0x1909||format==0x1906?1:0;
        unsigned bpp=type==0x1401?components:type==0x8363||type==0x8033||type==0x8034?2:0;
        if(!bpp || !components)return 0;
        row=(size_t)w*bpp; size=row*h;
        /* Packs are keyed on the packed upload bytes. Do not hash GL padding. */
        if(g_unpack_align>1 && row%g_unpack_align)return 0;
    }
    if(size>128u*1024u*1024u)return 0;
    u64 hash=ds_texture_hash(w,h,format,data,size);
    if(getenv("DS_HD_DIAG"))fprintf(stderr,"[HD_LOOKUP] %016llx %ux%u fmt=%x bytes=%zu\n",(unsigned long long)hash,w,h,format,size);
    char path[4096];int len=snprintf(path,sizeof path,"%s/%016llx.ktx",root,(unsigned long long)hash);
    if(len<0 || (size_t)len>=sizeof path)return 0;
    FILE* f=fopen(path,"rb");if(!f)return 0;
    if(fseek(f,0,SEEK_END)){fclose(f);return 0;}long length=ftell(f);rewind(f);
    if(length<64 || length>128*1024*1024){fclose(f);return 0;}
    u8* file=malloc((size_t)length);if(!file){fclose(f);return 0;}
    int read_ok=fread(file,1,length,f)==(size_t)length;fclose(f);DsKtx k;
    if(!read_ok || !ds_ktx_parse(file,length,&k)) { fprintf(stderr,"[HD_REJECT] invalid KTX %s\n",path);free(file);return 0; }
    s32 maximum=0;dgl_glGetIntegerv(0xd33,&maximum);
    if(k.width>(u32)maximum || k.height>(u32)maximum || !dgl_glTexImage2D || !dgl_glPixelStorei){free(file);return 0;}
    /* Allocate the largest decoded level before changing GPU state. */
    u8* decoded=ds_hd_decode(k.data[0],k.width,k.height,k.format);
    u8* original=malloc(size);
    if(!decoded || !original){free(decoded);free(original);free(file);return 0;}
    memcpy(original,data,size);
    if(!t){s32 id=0;dgl_glGetIntegerv(0x8069,&id);if(id<=0){free(original);free(decoded);free(file);return 0;}
        t=calloc(1,sizeof *t);if(!t){free(original);free(decoded);free(file);return 0;} t->id=id;t->next=ds_hd_textures;ds_hd_textures=t;}
    t->w=w;t->h=h;t->internal=internal;t->format=format;t->type=type;t->compressed=compressed;t->bytes=size;t->original=original;t->replaced=1;
    u32 mw=k.width,mh=k.height;int ok=1;
    dgl_glPixelStorei(0x0cf5,1);
    for(u32 l=0;l<k.levels;l++) {
        if(l)decoded=ds_hd_decode(k.data[l],mw,mh,k.format);
        if(!decoded){ok=0;break;}
        dgl_glTexImage2D(target,l,0x1908,mw,mh,0,0x1908,0x1401,decoded);free(decoded);
        mw=mw>1?mw/2:1;mh=mh>1?mh/2:1;
    }
    dgl_glPixelStorei(0x0cf5,g_unpack_align);
    if(ok && dgl_glTexParameteri)dgl_glTexParameteri(target,0x813d,k.levels-1);
    if(ok)fprintf(stderr,"[HD_TEXTURE] %016llx %ux%u -> %ux%u mips=%u format=%x\n",(unsigned long long)hash,w,h,k.width,k.height,k.levels,k.format);
    else ds_hd_restore(t);
    free(file);return ok;
}
