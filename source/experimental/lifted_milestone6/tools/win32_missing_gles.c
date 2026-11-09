/* GLES1 OpenGL fixed-point query compatibility for native Windows i686.
 * Standalone host import definitions for symbols missing from the owner build.
 * Uses desktop OpenGL 1.1 entrypoints provided by opengl32.dll.
 * No original EA source or asset data is included.
 */
#include <GL/gl.h>
#include <stdint.h>
#include <limits.h>
static s32 ds_gl_fixed(float x) {
    double y = (double)x * 65536.0;
    if (y >= 2147483647.0) return INT32_MAX;
    if (y <= -2147483648.0) return INT32_MIN;
    return (s32)(y < 0.0 ? y - 0.5 : y + 0.5);
}
static void ds_gl_copy_fixed(s32 *out,const float *in,int count) {
    for (int i=0;i<count;i++) out[i]=ds_gl_fixed(in[i]);
}
static int ds_gl_texenv_enum(u32 p) {
    return p==0x2200 || p==0x8571 || p==0x8572 ||
           (p>=0x8580 && p<=0x8582) || (p>=0x8588 && p<=0x858a) ||
           (p>=0x8590 && p<=0x8592) || (p>=0x8598 && p<=0x859a);
}
IMP(glGetTexParameterxv) {
    T(glGetTexParameterxv);
    if (!r2) RET(0);
    s32 *out=(s32*)P(r2);
    switch(r1) {
        case 0x2800: case 0x2801: case 0x2802: case 0x2803: case 0x8191:
            glGetTexParameteriv((GLenum)r0,(GLenum)r1,(GLint*)out); break;
        default: {
            GLfloat v[4]={0};
            int count=r1==0x1004 ? 4 : 1;
            glGetTexParameterfv((GLenum)r0,(GLenum)r1,v);
            ds_gl_copy_fixed(out,v,count);
        }
    }
    RET(0);
}
IMP(glGetTexEnvxv) {
    T(glGetTexEnvxv);
    if (!r2) RET(0);
    s32 *out=(s32*)P(r2);
    if (ds_gl_texenv_enum(r1))
        glGetTexEnviv((GLenum)r0,(GLenum)r1,(GLint*)out);
    else {
        GLfloat v[4]={0};
        int count=r1==0x2201 ? 4 : 1;
        glGetTexEnvfv((GLenum)r0,(GLenum)r1,v);
        ds_gl_copy_fixed(out,v,count);
    }
    RET(0);
}
IMP(glGetMaterialxv) {
    T(glGetMaterialxv);
    if (!r2) RET(0);
    GLfloat v[4]={0};
    glGetMaterialfv((GLenum)r0,(GLenum)r1,v);
    ds_gl_copy_fixed((s32*)P(r2),v,r1==0x1601 ? 1 : 4);
    RET(0);
}
IMP(glGetLightxv) {
    T(glGetLightxv);
    if (!r2) RET(0);
    GLfloat v[4]={0};
    int count=r1==0x1204 ? 3 :
       ((r1==0x1205 || r1==0x1206 || r1==0x1207 ||
         r1==0x1208 || r1==0x1209) ? 1 : 4);
    glGetLightfv((GLenum)r0,(GLenum)r1,v);
    ds_gl_copy_fixed((s32*)P(r2),v,count);
    RET(0);
}
IMP(glGetFixedv) {
    T(glGetFixedv);
    if (!r1) RET(0);
    int count=1;
    switch(r0) {
        case 0x0BA6: case 0x0BA7: case 0x0BA8: count=16; break;
        case 0x0B00: case 0x0B01: case 0x0B02: case 0x0B10:
        case 0x0C22: case 0x0C23: case 0x0BA2: count=4; break;
        case 0x0D3A: case 0x0B70: count=2; break;
        case 0x0B12: count=3; break;
    }
    GLfloat v[16]={0};
    glGetFloatv((GLenum)r0,v);
    ds_gl_copy_fixed((s32*)P(r1),v,count);
    RET(0);
}
IMP(glGetClipPlanef) {
    T(glGetClipPlanef);
    if (!r1) RET(0);
    GLdouble v[4]={0};
    glGetClipPlane((GLenum)r0,v);
    GLfloat *out=(GLfloat*)P(r1);
    for (int i=0;i<4;i++) out[i]=(GLfloat)v[i];
    RET(0);
}
IMP(glGetClipPlanex) {
    T(glGetClipPlanex);
    if (!r1) RET(0);
    GLdouble v[4]={0};
    glGetClipPlane((GLenum)r0,v);
    s32 *out=(s32*)P(r1);
    for (int i=0;i<4;i++) out[i]=ds_gl_fixed((float)v[i]);
    RET(0);
}
IMP(glPointSizePointerOES) {
    T(glPointSizePointerOES);
    /* Desktop GL2.1 has no fixed-function per-vertex point-size array.
     * Keep a visible warning, use first value as a best-effort point size. */
    static int warned;
    if (!warned) {
        fprintf(stderr,"[GLES] glPointSizePointerOES: per-vertex point sizes unsupported; using first point size only\n");
        warned=1;
    }
    if (r2) {
        float sz=r0==0x140c ? (float)(s32)(*(u32*)P(r2))/65536.0f :
                 r0==0x1406 ? *(float*)P(r2) : 1.0f;
        glPointSize(sz);
    }
    RET(0);
}
