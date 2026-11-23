"""Compare the released renderer's paper region to the 2007 V3 chart's photo cells.

Usage: python tests/drawing-reference.py downloaded-chart.gif build/rc2/rendered
Export the native-size PGMs with build/host-display[.exe] OUTPUT_DIRECTORY.
Source and chart digest are documented in docs/rc2/ANIMATIONS.md.
"""
import argparse
import hashlib
from pathlib import Path

from PIL import Image, ImageChops


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("chart", type=Path)
parser.add_argument("rendered", type=Path)
args = parser.parse_args()
expected = "44cd47c4d8a82eacd626d7a71e5899eb10f1f7ffd26c06316f7ef5fcca2bc79d"
assert hashlib.sha256(args.chart.read_bytes()).hexdigest() == expected
chart = Image.open(args.chart).convert("RGB")

# Each row has three named characters. Zero denotes a costume or the chart legend.
rows = [
    (49, 109, (19, 21, 37)), (112, 174, (20, 22, 24)),
    (179, 244, (33, 23, 26)), (246, 312, (0, 31, 44)),
    (315, 382, (35, 42, 28)), (387, 450, (0, 30, 29)),
    (454, 515, (18, 39, 25)), (519, 583, (45, 38, 43)),
    (590, 653, (36, 41, 50)), (662, 725, (27, 40, 49)),
    (731, 792, (47, 46, 48)), (798, 855, (34, 32, 0)),
]
columns = [(93, 157), (312, 377), (522, 588)]
verified = 0
for top, bottom, characters in rows:
    for character, (left, right) in zip(characters, columns):
        if not character:
            continue
        cell = chart.crop((left, top, right, bottom))
        green = Image.new("1", cell.size)
        green.putdata([pixel == (0, 255, 128) for pixel in cell.getdata()])
        if green.getbbox() is None:
            continue  # the chart author designates yellow cells as memory reconstructions
        paper = cell.crop(green.getbbox())
        ink = Image.new("L", paper.size)
        ink.putdata([255 if pixel == (0, 0, 0) else 0 for pixel in paper.getdata()])
        ink = ink.crop(ink.getbbox())
        x, y = 160 - ink.width // 2, 93
        display = Image.open(args.rendered / f"drawing-{character:02d}.pgm")
        actual = ImageChops.invert(display.crop((x, y, x + ink.width, y + ink.height)))
        assert ImageChops.difference(ink, actual).getbbox() is None, character
        verified += 1
assert verified == 23
print(f"PASS {verified} photo-transcribed V3 drawings, zero differing paper pixels")

