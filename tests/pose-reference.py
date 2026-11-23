"""Compare native-size meal scenes against the fixed eating cells on the V3 photo sheets.
Usage: python tests/pose-reference.py build/fidelity build/fidelity/rendered
Reads original image rectangles independently of the production extractor.
"""
import json
import re
import sys
from pathlib import Path
from PIL import Image, ImageChops, ImageOps

cache, rendered = map(Path, sys.argv[1:])
assets = {item["title"]: item["id"] for group in json.loads(
    (cache/"sheets.json").read_bytes())["v3"]["sections"] for item in group["items"]}
aliases = {"Teletchi (Boy)": "Teletchi (male)", "Teletchi (Girl)": "Teletchi (female)",
           "Patapatatchi": "Batabatchi", "Leaftchi": "Violetchi", "Dekotchi": "Decotchi"}
roster = re.findall(r'V3_CHAR\(\s*(\d+), [A-Z_0-9]+, "([^"]+)"',
    (Path(__file__).resolve().parents[1]/"src/core/v3-roster.inc").read_text())
count = 0
for number, name in roster:
    number = int(number)
    if number > 50:
        continue
    source = Image.open(cache/"sheets"/(assets[aliases.get(name,name)]+".webp")).convert("RGB")
    left, top, right, bottom = ((3,147,59,166) if number<4 else
                                (3,225,87,258) if number<18 else (4,295,104,337))
    midpoint = (left+right)//2
    for label, a, b in [("a",left,midpoint), ("b",midpoint,right)]:
        cell=source.crop((a,top,b,bottom))
        ink=Image.new("L",cell.size)
        ink.putdata([255 if max(rgb)<96 else 0 for rgb in cell.getdata()])
        assert ink.getbbox(), (name,label)
        ink=ink.crop(ink.getbbox())
        scale=min(3,60//ink.height)
        ink=ink.resize((ink.width*scale,ink.height*scale),Image.Resampling.NEAREST)
        native=Image.open(rendered/f"sheet-eat-{label}-{number:02d}.pgm")
        # Check a larger blank surround too, so a partial glyph cannot pass.
        displayed=ImageOps.invert(native.crop((146,103,146+72,103+60)))
        expected=Image.new("L",displayed.size)
        expected.paste(ink,(0,0))
        assert not ImageChops.difference(displayed,expected).getbbox(), (name,label,ink.size)
        count+=1
assert count==98
print(f"PASS {count} eating frames from 49 identities match V3 sheet pixels on the rendered LCD")

# The miscellaneous sheet shows a small skull, a large skull and a tooth in one strip.
others=Image.open(cache/"sheets"/"144777.webp").convert("RGB")
for index,cell in enumerate(((267,105,277,117),(279,105,291,117),(293,105,305,117))):
    pixels=others.crop(cell)
    ink=Image.new("L",pixels.size)
    ink.putdata([255 if max(rgb)<96 else 0 for rgb in pixels.getdata()])
    ink=ink.crop(ink.getbbox())
    ink=ink.resize((ink.width*2,ink.height*2),Image.Resampling.NEAREST)
    native=Image.open(rendered/f"illness-icon-{index:02d}.pgm")
    actual=ImageOps.invert(native.crop((33,112,53,132)))
    expected=Image.new("L",actual.size);expected.paste(ink,(0,0))
    assert not ImageChops.difference(actual,expected).getbbox(), ("illness",index)
print("PASS baby skull, adult skull and tooth match three separate original LCD glyphs")
