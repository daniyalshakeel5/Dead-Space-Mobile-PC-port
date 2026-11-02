#!/usr/bin/env bash
# Reassembles separately downloaded 24 MiB original-free QEMU Linux segments.
set -Eeuo pipefail
cd -- "$(dirname "$0")"
BUNDLE=DeadSpaceMobile-Xperia-Linux-x86_64-QEMU.zip
fail() {
  printf 'Download problem: %s\n' "$1" >&2
  if command -v zenity >/dev/null 2>&1; then
    zenity --error --title="Dead Space Linux setup" --text="$1" 2>/dev/null || :
  elif command -v kdialog >/dev/null 2>&1; then
    kdialog --error "$1" 2>/dev/null || :
  fi
  exit 1
}
[[ -f PARTS-SHA256SUMS.txt && -f BUNDLE-SHA256SUMS.txt ]] ||
  fail "Extract 00-START-HERE.zip alongside all numbered download parts."
command -v sha256sum >/dev/null || fail "Linux sha256sum utility is missing."
command -v unzip >/dev/null || fail "Linux unzip utility is missing."
echo "Checking every downloaded part..."
sha256sum -c PARTS-SHA256SUMS.txt ||
  fail "A download is missing or damaged. Re-download only the part marked FAILED."
[[ ! -e DeadSpaceMobile-Xperia-Linux ]] ||
  fail "DeadSpaceMobile-Xperia-Linux already exists. Move it before extracting again."
echo "Reassembling the verified QEMU Linux runtime..."
cat "$BUNDLE".part.* > "$BUNDLE".tmp ||
  fail "Could not join the parts. Check available disk space."
mv -- "$BUNDLE".tmp "$BUNDLE"
sha256sum -c BUNDLE-SHA256SUMS.txt ||
  fail "Combined ZIP checksum mismatch. Do not run this archive."
unzip -tqq "$BUNDLE" || fail "Combined archive failed ZIP integrity testing."
unzip -q "$BUNDLE" || fail "Could not extract the Linux runtime."
MESSAGE="Linux QEMU tester is ready. Open DeadSpaceMobile-Xperia-Linux. Put your OWN deadspace.zip beside Run Dead Space.sh, then choose Run as Program."
printf '\n%s\n' "$MESSAGE"
if command -v zenity >/dev/null 2>&1; then
  zenity --info --title="Dead Space Linux setup" --text="$MESSAGE" 2>/dev/null || :
elif command -v kdialog >/dev/null 2>&1; then
  kdialog --msgbox "$MESSAGE" 2>/dev/null || :
fi
