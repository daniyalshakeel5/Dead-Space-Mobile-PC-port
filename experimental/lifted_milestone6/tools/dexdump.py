import sys, logging
from loguru import logger
logger.remove()
from androguard.core.dex import DEX
d = DEX(open('../apk/classes.dex','rb').read())
def dump(cname, only=None, maxins=80):
    c = d.get_class(cname)
    if not c: print("no", cname); return
    print("=====", cname, "extends", c.get_superclassname())
    for m in c.get_methods():
        if only and m.get_name() not in only: continue
        code = m.get_code()
        print(f"-- {m.get_name()}{m.get_descriptor()}", "(native)" if not code else "")
        if not code: continue
        n = 0
        for ins in code.get_bc().get_instructions():
            print("    ", ins.get_name(), ins.get_output()[:120]); n += 1
            if n >= maxins: print("     ..."); break
if __name__ == "__main__":
    if sys.argv[1] == "list":
        print([c.get_name() for c in d.get_classes() if sys.argv[2].lower() in c.get_name().lower()])
    else:
        dump(sys.argv[1], set(sys.argv[2:]) or None, 120)
