#!/usr/bin/env python3
"""Reject a stationary corridor in the scripted first-movement smoke test.

Use only with the documented 1280x720 input schedule: movement at frames
18500..19500, camera input after 19800. This gate detects the known no-movement
regression; it does not replace screenshot review or prove general playability.
Requires Pillow (already installed by the gameplay capture workflows).
"""
import argparse
from pathlib import Path
from PIL import Image, ImageChops, ImageStat

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('evidence', type=Path)
args = parser.parse_args()
root = args.evidence
exit_code = (root / 'exit-code.txt').read_text(encoding='utf-8-sig').strip()
if exit_code != '0':
    raise SystemExit(f'FAIL: runtime exit {exit_code}')
with Image.open(root / 'f18000.png') as before, Image.open(root / 'f19500.png') as after:
    if before.size != (1280, 720) or after.size != (1280, 720):
        raise SystemExit('FAIL: movement gate requires 1280x720 captures')
    # Right corridor background excludes the character and animated tutorial.
    region = (800, 400, 1250, 700)
    delta = ImageChops.difference(before.convert('RGB').crop(region), after.convert('RGB').crop(region))
    mean_delta = sum(ImageStat.Stat(delta).mean) / 3
print(f'Corridor background mean absolute difference: {mean_delta:.6f}')
if mean_delta < 5:
    raise SystemExit('FAIL: movement did not produce the expected corridor change')
print('PASS: scene changed during movement-only interval; inspect captures to confirm movement')
