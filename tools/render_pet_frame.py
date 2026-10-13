"""Render PET1 diagnostic records from the actual portrait OLED framebuffer.

Requires Pillow. Example: python render_pet_frame.py frame.log preview.png
The firmware always switches to the pet page before exporting pixels.
"""
import argparse
from pathlib import Path
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument('log', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
frames = [line[5:] for line in args.log.read_text(encoding='utf-8').splitlines()
          if line.startswith('PET1 ')]
if not frames:
    raise SystemExit('No PET1 framebuffer in log')
data = bytes.fromhex(frames[-1])
if len(data) != 1024:
    raise SystemExit('Expected 1024 OLED framebuffer bytes')
image = Image.new('1', (128, 64))
pixels = image.load()
for y in range(64):
    for x in range(128):
        pixels[x, y] = bool(data[x+(y//8)*128] & (1 << (y % 8)))
# Adafruit GFX rotation 1 maps portrait (x,y) to physical (127-y,x).
image = image.rotate(90, expand=True)
image.resize((512, 1024), Image.Resampling.NEAREST).convert('RGB').save(args.output)
print('Rendered {} portrait OLED pixels'.format(image.size))
