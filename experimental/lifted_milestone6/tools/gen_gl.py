#!/usr/bin/env python3
"""
Generates gl_gen.c: guest-facing GLES 1.x shims (imp_gl*) that forward to desktop OpenGL.

Desktop functions are described once (return letter, parameter letters). Letters:
  u = GLuint/GLenum/GLbitfield   i = GLint/GLsizei   f = GLfloat   d = GLdouble   b = GLboolean   p = pointer
Guest parameter letters add:   x = GLfixed (16.16) -> converted to float/double as the desktop side needs.
Guest arguments follow the ARM EABI softfp rules: every argument is one 32-bit word (r0-r3, then stack);
floats travel as raw bits.
"""
import sys

D = {  # desktop function: (return, params)
 "glActiveTexture": ("v", "u"), "glAlphaFunc": ("v", "uf"), "glBindBuffer": ("v", "uu"), "glBindTexture": ("v", "uu"),
 "glBlendFunc": ("v", "uu"), "glBlendFuncSeparate": ("v", "uuuu"), "glBlendEquation": ("v", "u"),
 "glBlendEquationSeparate": ("v", "uu"), "glBufferData": ("v", "uipu"), "glBufferSubData": ("v", "uiip"),
 "glClear": ("v", "u"), "glClearColor": ("v", "ffff"), "glClearDepth": ("v", "d"), "glClearStencil": ("v", "i"),
 "glClientActiveTexture": ("v", "u"), "glColor4f": ("v", "ffff"), "glColor4ub": ("v", "bbbb"),
 "glColorMask": ("v", "bbbb"), "glColorPointer": ("v", "iuip"), "glCopyTexImage2D": ("v", "uiuiiiii"),
 "glCopyTexSubImage2D": ("v", "uiiiiiii"), "glCullFace": ("v", "u"), "glDeleteBuffers": ("v", "ip"),
 "glDeleteTextures": ("v", "ip"), "glDepthFunc": ("v", "u"), "glDepthMask": ("v", "b"), "glDepthRange": ("v", "dd"),
 "glDisable": ("v", "u"), "glDisableClientState": ("v", "u"), "glDrawArrays": ("v", "uii"),
 "glDrawElements": ("v", "uiup"), "glEnable": ("v", "u"), "glEnableClientState": ("v", "u"), "glFinish": ("v", ""),
 "glFlush": ("v", ""), "glFogf": ("v", "uf"), "glFogfv": ("v", "up"), "glFrontFace": ("v", "u"),
 "glFrustum": ("v", "dddddd"), "glGenBuffers": ("v", "ip"), "glGenTextures": ("v", "ip"),
 "glGetBooleanv": ("v", "up"), "glGetBufferParameteriv": ("v", "uup"), "glGetError": ("u", ""),
 "glGetFloatv": ("v", "up"), "glGetIntegerv": ("v", "up"), "glGetLightfv": ("v", "uup"),
 "glGetMaterialfv": ("v", "uup"), "glGetPointerv": ("v", "up"), "glGetString": ("p", "u"),
 "glGetTexEnvfv": ("v", "uup"), "glGetTexEnviv": ("v", "uup"), "glGetTexParameterfv": ("v", "uup"),
 "glGetTexParameteriv": ("v", "uup"), "glHint": ("v", "uu"), "glIsBuffer": ("b", "u"), "glIsEnabled": ("b", "u"),
 "glIsTexture": ("b", "u"), "glLightModelf": ("v", "uf"), "glLightModelfv": ("v", "up"), "glLightf": ("v", "uuf"),
 "glLightfv": ("v", "uup"), "glLineWidth": ("v", "f"), "glLoadIdentity": ("v", ""), "glLoadMatrixf": ("v", "p"),
 "glLogicOp": ("v", "u"), "glMaterialf": ("v", "uuf"), "glMaterialfv": ("v", "uup"), "glMatrixMode": ("v", "u"),
 "glMultMatrixf": ("v", "p"), "glMultiTexCoord4f": ("v", "uffff"), "glNormal3f": ("v", "fff"),
 "glNormalPointer": ("v", "uip"), "glOrtho": ("v", "dddddd"), "glPixelStorei": ("v", "ui"),
 "glPointParameterf": ("v", "uf"), "glPointParameterfv": ("v", "up"), "glPointSize": ("v", "f"),
 "glPolygonOffset": ("v", "ff"), "glPopMatrix": ("v", ""), "glPushMatrix": ("v", ""),
 "glReadPixels": ("v", "iiiiuup"), "glRotatef": ("v", "ffff"), "glSampleCoverage": ("v", "fb"),
 "glScalef": ("v", "fff"), "glScissor": ("v", "iiii"), "glShadeModel": ("v", "u"), "glStencilFunc": ("v", "uiu"),
 "glStencilMask": ("v", "u"), "glStencilOp": ("v", "uuu"), "glTexCoordPointer": ("v", "iuip"),
 "glTexEnvf": ("v", "uuf"), "glTexEnvfv": ("v", "uup"), "glTexEnvi": ("v", "uui"), "glTexEnviv": ("v", "uup"),
 "glTexGenf": ("v", "uuf"), "glTexGeni": ("v", "uui"), "glTexGenfv": ("v", "uup"), "glTexGeniv": ("v", "uup"),
 "glTexImage2D": ("v", "uiiiiiuup"), "glTexParameterf": ("v", "uuf"), "glTexParameterfv": ("v", "uup"),
 "glTexParameteri": ("v", "uui"), "glTexParameteriv": ("v", "uup"), "glTexSubImage2D": ("v", "uiiiiiuup"),
 "glTranslatef": ("v", "fff"), "glVertexPointer": ("v", "iuip"), "glViewport": ("v", "iiii"),
 "glMapBuffer": ("p", "uu"), "glUnmapBuffer": ("b", "u"), "glClipPlane": ("v", "up"),
 "glBindFramebufferEXT": ("v", "uu"), "glBindRenderbufferEXT": ("v", "uu"), "glDeleteFramebuffersEXT": ("v", "ip"),
 "glDeleteRenderbuffersEXT": ("v", "ip"), "glFramebufferRenderbufferEXT": ("v", "uuuu"),
 "glFramebufferTexture2DEXT": ("v", "uuuui"), "glGenFramebuffersEXT": ("v", "ip"),
 "glGenRenderbuffersEXT": ("v", "ip"), "glCheckFramebufferStatusEXT": ("u", "u"),
 "glRenderbufferStorageEXT": ("v", "uuii"), "glIsFramebufferEXT": ("b", "u"), "glIsRenderbufferEXT": ("b", "u"),
 "glGetRenderbufferParameterivEXT": ("v", "uup"), "glGetFramebufferAttachmentParameterivEXT": ("v", "uuup"),
 "glGenerateMipmapEXT": ("v", "u"),
}

# guest name, guest param letters, desktop function ("" = same name)
S = []
def same(*names):
    for n in names: S.append((n, D[n][1], n))
same("glActiveTexture", "glAlphaFunc", "glBindBuffer", "glBindTexture", "glBlendFunc", "glBufferData", "glBufferSubData",
     "glClear", "glClearColor", "glClearStencil", "glClientActiveTexture", "glColor4f", "glColor4ub", "glColorMask",
     "glColorPointer", "glCopyTexImage2D", "glCopyTexSubImage2D", "glCullFace", "glDeleteBuffers", "glDeleteTextures",
     "glDepthFunc", "glDepthMask", "glDisableClientState", "glDrawArrays", "glDrawElements", "glEnableClientState",
     "glFinish", "glFlush", "glFogf", "glFogfv", "glFrontFace", "glGenBuffers", "glGenTextures", "glGetBooleanv",
     "glGetBufferParameteriv", "glGetError", "glGetFloatv", "glGetLightfv", "glGetMaterialfv", "glGetPointerv",
     "glGetTexEnvfv", "glGetTexEnviv", "glGetTexParameterfv", "glGetTexParameteriv", "glHint", "glIsBuffer",
     "glIsTexture", "glLightModelf", "glLightModelfv", "glLightf", "glLightfv", "glLineWidth", "glLoadIdentity",
     "glLoadMatrixf", "glLogicOp", "glMaterialf", "glMaterialfv", "glMatrixMode", "glMultMatrixf", "glMultiTexCoord4f",
     "glNormal3f", "glNormalPointer", "glPointParameterf", "glPointParameterfv", "glPointSize",
     "glPolygonOffset", "glPopMatrix", "glPushMatrix", "glReadPixels", "glRotatef", "glSampleCoverage", "glScalef",
     "glScissor", "glShadeModel", "glStencilFunc", "glStencilMask", "glStencilOp", "glTexCoordPointer", "glTexEnvf",
     "glTexEnvfv", "glTexEnvi", "glTexEnviv", "glTexImage2D", "glTexParameterf", "glTexParameterfv", "glTexParameteri",
     "glTexParameteriv", "glTexSubImage2D", "glTranslatef", "glVertexPointer", "glViewport", "glIsEnabled")
S += [
 ("glClearDepthf", "f", "glClearDepth"), ("glDepthRangef", "ff", "glDepthRange"), 
 ("glOrthof", "ffffff", "glOrtho"),
 ("glBlendFuncSeparateOES", "uuuu", "glBlendFuncSeparate"), ("glBlendEquationOES", "u", "glBlendEquation"),
 ("glBlendEquationSeparateOES", "uu", "glBlendEquationSeparate"),
 # fixed point
 ("glAlphaFuncx", "ux", "glAlphaFunc"), ("glClearColorx", "xxxx", "glClearColor"), ("glClearDepthx", "x", "glClearDepth"),
 ("glColor4x", "xxxx", "glColor4f"), ("glDepthRangex", "xx", "glDepthRange"), 
 ("glLightx", "uux", "glLightf"), ("glLineWidthx", "x", "glLineWidth"), ("glLoadMatrixx", "p", "@loadmatrixx"),
 ("glMaterialx", "uux", "glMaterialf"), ("glMultMatrixx", "p", "@multmatrixx"), ("glMultiTexCoord4x", "uxxxx", "glMultiTexCoord4f"),
 ("glNormal3x", "xxx", "glNormal3f"), ("glOrthox", "xxxxxx", "glOrtho"), ("glPointParameterx", "ux", "glPointParameterf"),
 ("glPointSizex", "x", "glPointSize"), ("glPolygonOffsetx", "xx", "glPolygonOffset"), ("glRotatex", "xxxx", "glRotatef"),
 ("glSampleCoveragex", "xb", "glSampleCoverage"), ("glScalex", "xxx", "glScalef"), ("glTranslatex", "xxx", "glTranslatef"),
 # OES framebuffer object -> EXT_framebuffer_object (same enum values)
 ("glBindFramebufferOES", "uu", "glBindFramebufferEXT"), ("glBindRenderbufferOES", "uu", "glBindRenderbufferEXT"),
 ("glDeleteFramebuffersOES", "ip", "glDeleteFramebuffersEXT"), ("glDeleteRenderbuffersOES", "ip", "glDeleteRenderbuffersEXT"),
 ("glFramebufferRenderbufferOES", "uuuu", "glFramebufferRenderbufferEXT"),
 ("glFramebufferTexture2DOES", "uuuui", "glFramebufferTexture2DEXT"), ("glGenFramebuffersOES", "ip", "glGenFramebuffersEXT"),
 ("glGenRenderbuffersOES", "ip", "glGenRenderbuffersEXT"), ("glCheckFramebufferStatusOES", "u", "glCheckFramebufferStatusEXT"),
 ("glRenderbufferStorageOES", "uuii", "glRenderbufferStorageEXT"), ("glIsFramebufferOES", "u", "glIsFramebufferEXT"),
 ("glIsRenderbufferOES", "u", "glIsRenderbufferEXT"), ("glGetRenderbufferParameterivOES", "uup", "glGetRenderbufferParameterivEXT"),
 ("glGetFramebufferAttachmentParameterivOES", "uuup", "glGetFramebufferAttachmentParameterivEXT"),
 ("glGenerateMipmapOES", "u", "glGenerateMipmapEXT"),
 ("glMapBufferOES", "uu", "glMapBuffer"), ("glUnmapBufferOES", "u", "glUnmapBuffer"),
]

CT = {"u": "u32", "i": "s32", "f": "float", "d": "double", "b": "u8", "p": "void*", "v": "void"}
FMT = {"u": "%u", "i": "%d", "f": "%g", "d": "%g", "b": "%u", "p": "%p"}

def W(k): return f"W({k})"
def conv(gl, dl, k):
    w = W(k)
    if gl == "x":
        return f"fxf({w})" if dl == "f" else f"((double)(s32){w} / 65536.0)"
    if gl == "f":
        return f"u2f({w})" if dl == "f" else f"((double)u2f({w}))"
    if dl == "p": return f"P({w})"
    return f"({CT[dl]}){w}"

def main(out):
    o = []
    o.append("// generated by gen_gl.py - do not edit\n")
    o.append("#ifdef _WIN32\n#define GLCALL __stdcall\n#else\n#define GLCALL\n#endif\n")
    o.append("static FILE* glogf; static int gl_have_backend;\n")
    for n, (r, ps) in D.items():
        params = ", ".join(CT[c] for c in ps) or "void"
        o.append(f"static {CT[r]} (GLCALL *dgl_{n})({params});\n")
    # loader
    o.append("static void gl_load_all(void* (*get)(const char*)) {\n  gl_have_backend = 1;\n")
    for n in D: o.append(f'  *(void**)&dgl_{n} = get("{n}");\n')
    o.append("}\n")
    # logging backend: every desktop call is printed, nothing is drawn
    for n, (r, ps) in D.items():
        params = ", ".join(f"{CT[c]} a{i}" for i, c in enumerate(ps)) or "void"
        fmt = ", ".join(FMT[c] for c in ps)
        args = "".join(f", {'(void*)a%d' % i if c == 'p' else 'a%d' % i}" for i, c in enumerate(ps))
        ret = "" if r == "v" else ("return 0;" if r != "p" else "return 0;")
        if n == "glGetString": ret = 'return (void*)"log-backend";'
        o.append(f'static {CT[r]} GLCALL lg_{n}({params}) {{ fprintf(glogf, "{n}({fmt})\\n"{args}); {ret} }}\n')
    o.append("static void gl_install_log_backend(FILE* f) {\n  glogf = f; gl_have_backend = 1;\n")
    for n in D: o.append(f"  *(void**)&dgl_{n} = (void*)lg_{n};\n")
    o.append("}\n")
    # shims
    done = set()
    for gname, gps, dname in S:
        if gname in done: continue
        done.add(gname)
        o.append(f"IMP({gname}) {{\n  T({gname});\n")
        # Genuine game sky/starfield diagnostics, disabled by default.
        if gname == "glBindTexture":
            o.append("  ds_sky_diag_bind(W(0), W(1));\n")
        elif gname == "glTexImage2D":
            o.append("  ds_sky_diag_image(W(0), W(3), W(4));\n")
        elif gname in ("glDrawElements", "glDrawArrays"):
            o.append("  ds_sky_diag_draw();\n")
        if dname.startswith("@"):                         # fixed-point matrices: 16 x 16.16 -> float[16]
            fn = "glLoadMatrixf" if dname == "@loadmatrixx" else "glMultMatrixf"
            o.append(f"  if (dgl_{fn}) {{ float m[16], patched[16]; const s32* s = (const s32*)P(r0); for (int i = 0; i < 16; i++) m[i] = fxf((u32)s[i]); dgl_{fn}(ds_matrix_filter(m, patched, \"{gname}\")); }}\n  RET(0);\n}}\n")
            continue
        if gname == "glMatrixMode":
            o.append("  ds_matrix_mode_set(r0);\n")
        if gname in ("glLoadMatrixf", "glMultMatrixf"):
            o.append(f'  if (dgl_{dname}) {{ float patched[16]; dgl_{dname}(ds_matrix_filter((const float*)P(r0), patched, "{gname}")); }}\n  RET(0);\n}}\n')
            continue
        r, dps = D[dname]
        if len(gps) != len(dps): raise SystemExit(f"arity mismatch {gname}")
        args = ", ".join(conv(g, d, k) for k, (g, d) in enumerate(zip(gps, dps)))
        call = f"dgl_{dname}({args})"
        if r == "v": o.append(f"  if (dgl_{dname}) {call};\n  RET(0);\n}}\n")
        elif r == "p": o.append(f"  RET(dgl_{dname} ? U({call}) : 0);\n}}\n")
        else: o.append(f"  RET(dgl_{dname} ? (u32){call} : 0);\n}}\n")
    open(out, "w").write("".join(o))
    print(f"{len(done)} generated shims, {len(D)} desktop functions -> {out}")
    # report which guest GL imports are NOT covered by generated or hand-written shims
    return done

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "gl_gen.c")
