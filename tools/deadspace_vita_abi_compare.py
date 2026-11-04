"""Compare user-confirmed APK ABI with Vita loader requirements.
Vita uses the Xperia Play ARMv6 v1.1.33 release; our build is not assumed
equivalent. Only hashes, counts, presence booleans and file names are emitted.
No original ELF body, APK or decompiled game logic is copied into this project.
"""
import argparse
import hashlib
import json
from pathlib import Path
from zipfile import ZipFile

from tools.deadspace_original_code_window import elf32_jni_symbols

VITA_LIBRARY_NAME = "lib/armeabi/libEAMGameDeadSpace.so"
VITA_LIBRARY_SHA1 = "0ed42b611415015807f759ec9b5457857143ce39"
# Entrypoints actually referenced by the Vita source loader/main.c and
# loader/reimpl/controls.c, not invented desktop JNI signatures.
VITA_REQUIRED_JNI = (
    "JNI_OnLoad",
    "Java_com_ea_EAIO_EAIO_Startup",
    "Java_com_ea_blast_MainActivity_NativeOnCreate",
    "Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceCreated",
    "Java_com_ea_blast_AndroidRenderer_NativeOnDrawFrame",
    "Java_com_ea_blast_KeyboardAndroid_NativeOnVisibilityChanged",
    "Java_com_ea_blast_TouchSurfaceAndroid_NativeOnPointerEvent",
    "Java_com_ea_blast_KeyboardAndroid_NativeOnKeyDown",
    "Java_com_ea_blast_KeyboardAndroid_NativeOnKeyUp",
    "Java_com_ea_blast_AccelerometerAndroidDelegate_NativeOnAcceleration",
)

def summarize(elf_name: str, elf_bytes: bytes, jni_symbols: set[str]) -> dict:
    same_name = elf_name == VITA_LIBRARY_NAME
    same_hash = hashlib.sha1(elf_bytes).hexdigest() == VITA_LIBRARY_SHA1
    matched = sorted(set(VITA_REQUIRED_JNI) & jni_symbols)
    missing = sorted(set(VITA_REQUIRED_JNI) - jni_symbols)
    return {
        "kind": "vita-loader-to-native-apk-abi-comparison",
        "schema": 1,
        "vita_repository": "https://github.com/v-atamanenko/deadspace-vita",
        "target_game_version": "Xperia Play 1.1.33 (ARMv6)",
        "library_member_matches_vita": same_name,
        "library_sha1_matches_vita": same_hash,
        "identical_vita_game_binary": bool(same_name and same_hash),
        "required_vita_jni_count": len(VITA_REQUIRED_JNI),
        "matching_jni_entrypoints": matched,
        "missing_jni_entrypoints": missing,
        "warning": "JNI symbol overlap does not establish original game-code or binary equivalence."
    }

def compare(apk: Path) -> dict:
    with ZipFile(apk) as z:
        libs = [item for item in z.infolist()
                if item.filename.startswith("lib/") and item.filename.endswith(".so")
                and 1024 < item.file_size < 64 * 1024 * 1024]
        if len(libs) != 1:
            raise ValueError("Cannot compare APK ABI: expected precisely one bounded .so")
        item = libs[0]
        with z.open(item) as stream:
            elf = stream.read(64 * 1024 * 1024 + 1)
        if len(elf) != item.file_size or len(elf) > 64 * 1024 * 1024:
            raise ValueError("Unexpected native library size")
    symbols = set(elf32_jni_symbols(elf, set(VITA_REQUIRED_JNI)))
    return summarize(item.filename, elf, symbols)

def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--apk", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    result = compare(args.apk)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True)+"\n")
    print("Vita ABI comparison finished: edition match:", result["identical_vita_game_binary"],
          "; JNI overlap:",len(result["matching_jni_entrypoints"]),"/",
          result["required_vita_jni_count"])
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
