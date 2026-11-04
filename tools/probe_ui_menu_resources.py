#!/usr/bin/env python3
import zipfile,sys
with zipfile.ZipFile(sys.argv[1]) as apk:
    names=apk.namelist()
    matches=[n for n in names if n.startswith("assets/") and
             any(word in n.lower() for word in
                ("menu","frontend","gui","interface","layout","script","scene","screen"))]
    for name in sorted(matches)[:250]: print("MENU_UI_ASSET",name)
    print("MENU_UI_ASSET_MATCHES",len(matches))
