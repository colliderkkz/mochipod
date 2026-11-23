"""Extract single-bit poses from the V3-only MasterPengo reference sheets.
Usage: python tools/v3-poses.py CACHE OUTPUT.inc REPORT.md [--download]
CACHE contains AM71's sprites.json, sheets.json and sheets/<id>.webp.
Only the named groups used by the player are compiled. Box labels follow the
sheet index in reading order. Group strips with multi-row layouts are explicit.
"""
import hashlib
import json
import re
import sys
from collections import Counter
from pathlib import Path

import numpy as np
from PIL import Image
from scipy.ndimage import find_objects, label

import argparse
from urllib.parse import quote
from urllib.request import urlopen

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("cache",type=Path)
parser.add_argument("output",type=Path)
parser.add_argument("report",type=Path)
parser.add_argument("--download",action="store_true",help="fetch missing V3 sheets and indexes from the AM71 mirror")
args=parser.parse_args()
cache,output,report=args.cache,args.output,args.report

def fetch_missing(name, target):
    if args.download and not target.is_file():
        target.parent.mkdir(parents=True, exist_ok=True)
        with urlopen("https://am71.ca/tama/"+quote(name),timeout=30) as source:
            data=source.read()
        target.write_bytes(data)

for index in ("sprites.json","sheets.json"):
    fetch_missing(index,cache/index)
root = Path(__file__).resolve().parents[1]
assets = json.loads((cache / "sheets.json").read_bytes())["v3"]["sections"]
groups = json.loads((cache / "sprites.json").read_bytes())["v3"]
states = """general happy back_turned sad eyes_closed angry eating no running_and_falling
badly_behaved rolling sick fishing kissing singing backpack bath brushing_teeth in_bed
close_up bow bow_tie cap darts drum hair_gel makeup music pen_pencil paper remote_control
roller_skates royal_costume shirt shoes sunglasses throne trumpet umbrella weights wig wings
birthday new_year winter easter halloween thanksgiving tree santa fireworks angel skull tooth egg hatching bags notes football droppings toilet wash shop_stall pass_stall""".split()
roster = {name: int(n) for n, name in re.findall(r'V3_CHAR\(\s*(\d+), [A-Z_0-9]+, "([^"]+)"', (root/"src/core/v3-roster.inc").read_text())}
roster.update({alias: roster[name] for alias, name in {
    "Teletchi (male)": "Teletchi (Boy)", "Teletchi (female)": "Teletchi (Girl)",
    "Batabatchi": "Patapatatchi", "Violetchi": "Leaftchi", "Decotchi": "Dekotchi"}.items()})
pixels = bytearray()
unique = {}
frames = [(0,0,0)]
sequences = {}
counts = Counter()
rows = []
excluded = []

def crops(mask, count):
    """Separate adjacent poses at the largest inter-sprite blank runs."""
    ys, xs = np.nonzero(mask)
    if not len(xs):
        return []
    mask = mask[min(ys):max(ys)+1, min(xs):max(xs)+1]
    edges = np.flatnonzero(np.diff(np.r_[False, ~mask.any(axis=0), False]))
    gaps = sorted(zip(edges[::2], edges[1::2]), key=lambda ab: ab[1]-ab[0], reverse=True)
    if len(gaps) < count-1 or any(b-a<3 for a,b in gaps[:count-1]):
        return []
    cuts = [0]+sorted((a+b)//2 for a,b in gaps[:count-1])+[mask.shape[1]]
    result = []
    for a,b in zip(cuts,cuts[1:]):
        part = mask[:,a:b]
        y,x = np.nonzero(part)
        if not len(x): return []
        part = part[min(y):max(y)+1,min(x):max(x)+1]
        if max(part.shape)>32: return []
        result.append(part)
    return result

def record(who, state, masks):
    if not masks: return
    entries = []
    for mask in masks:
        h,w = mask.shape
        data = np.packbits(mask.ravel()).tobytes()
        key=(w,h,data)
        if key not in unique:
            unique[key]=len(frames)
            frames.append((len(pixels),w,h))
            pixels.extend(data)
        entries.append(unique[key])
    sequences[who,state]=entries
    counts[who]+=len(entries)

for section in assets:
    for asset in section["items"]:
        name=asset["title"]
        who=roster.get(name,0)
        if not who and name not in ("Calendar Events","Others"):continue
        key=re.sub(r'[()]','',name.replace(' ','_'))
        meta=next(cat[key]["states"] for cat in groups.values() if key in cat)
        path=cache/"sheets"/(asset["id"]+".webp")
        fetch_missing(asset["src"],path)
        rgb=np.asarray(Image.open(path).convert("RGB"))
        if name=="Others":
            def screen(x,y,w,h):
                return (rgb[y:y+h,x:x+w].max(axis=2)<96)
            record(0,"fireworks",[screen(9,281,32,30),screen(47,281,32,30)])
            record(0,"angel",crops(screen(284,26,22,18),1))
            # Fixed cells on the Others sheet. Separate baby and normal skulls;
            # every bounds tuple excludes the magenta/blue chart border.
            for state, bounds in {
                "skull":[(268,108,8,7),(280,106,9,10)], "tooth":[(294,105,10,11)],
                "egg":[(272,7,13,13),(290,9,14,11)],
                "bags":[(35,166,10,12),(48,166,10,12)],
                "notes":[(61,168,7,10),(71,168,10,10)],
                "football":[(69,218,13,13)],
                "droppings":[(282,89,10,9),(294,89,11,9)],
                "toilet":[(10,334,26,21),(40,325,22,29)],"wash":[(71,321,9,32)],
                "shop_stall":[(9,8,32,30),(47,8,32,30),(9,42,32,30),(47,42,32,30)],
                "pass_stall":[(50,84,32,30)]
            }.items():
                for_bounds=[crops(screen(*cell),1) for cell in bounds]
                assert all(len(part)==1 for part in for_bounds),state
                record(0,state,[part[0] for part in for_bounds])
            rows.append((name,asset["page_url"],hashlib.sha256(path.read_bytes()).hexdigest(),2,""))
            continue
        boxes=sorted((x.start,y.start,x.stop,y.stop) for y,x in find_objects(label((rgb==[165,28,210]).all(axis=2))[0]) if x.stop-x.start>=30 and y.stop-y.start>=10)
        # Slightly misaligned groups in one column still read top to bottom.
        ordered=[]
        while boxes:
            col=[b for b in boxes if b[0]<=boxes[0][0]+6]
            ordered.extend(sorted(col,key=lambda b:b[1]))
            boxes=boxes[len(col):]
        assert len(ordered)==len(meta),(name,len(ordered),len(meta))
        skipped=[]
        for (x,y,r,b),(state,info) in zip(ordered,meta.items()):
            if who>=53 and state!="general":continue
            if state not in states and state!="christmas":continue
            cell=rgb[y+1:b-1,x+1:r-1]
            mask=(cell.max(axis=2)<96)
            n=len(info["frames"])
            if state=="pen_pencil":
                # Two writing poses above the finished paper.
                record(who,"paper",crops(mask[len(mask)//2:],1))
                mask=mask[:len(mask)//2];n=2
            if state=="close_up":
                mask=mask[len(mask)//2:];n=2
            if state in ("birthday","new_year"):
                # Frames are inside blue LCD outlines. Blue borders aren't ink.
                if state=="birthday":mask=mask[len(mask)//2:]
                n=2
            if state=="christmas":
                record(who,"tree",crops(mask[:len(mask)//2],2))
                record(who,"santa",crops(mask[len(mask)//2:],3))
                continue
            if state=="roller_skates" and len(mask)>50: continue
            parts=crops(mask,n)
            if parts:record(who,state,parts)
            else:skipped.append(state)
        if who:
            unused=[state for state in meta if (who,state) not in sequences]
            excluded.append((name,unused))
        rows.append((name,asset["page_url"],hashlib.sha256(path.read_bytes()).hexdigest(),counts[who],', '.join(skipped)))
offsets=[0]
refs=[]
for who in range(61):
    for state in states:refs.extend(sequences.get((who,state),[]));offsets.append(len(refs))
def numbers(stream,name,kind,values,width):
    stream.write(f"static const {kind} {name}[]={{\n")
    for i in range(0,len(values),width):stream.write("    "+",".join(map(str,values[i:i+width]))+",\n")
    stream.write("};\n")
with output.open("w",newline="\n") as f:
    f.write("/* V3 photographic transcriptions by MasterPengo, sheet archive by AM71. See docs/fidelity/POSES.md. */\n")
    f.write("enum SheetPose {"+", ".join("SP_"+k.upper() for k in states)+", SP_COUNT };\n")
    numbers(f,"pose_ink","uint8_t",pixels,24)
    f.write("static const struct {uint32_t offset;uint8_t w,h;} pose_frames[]={\n")
    for off,w,h in frames:f.write(f"    {{{off},{w},{h}}},\n")
    f.write("};\n")
    numbers(f,"pose_ids","uint16_t",refs,18)
    numbers(f,"pose_offsets","uint16_t",offsets,18)
with report.open("w",encoding="utf-8") as f:
    f.write("# V3 动作参考表\n\nMasterPengo 按原机照片转录，AM71 提供版本分栏和画表镜像。角色图、节令图和杂项使用 V3 栏。节拍未经原机标定。镜像文件摘要用于复查同一底图。\n\n")
    f.write(f"{len(refs)} 个帧引用，{len(frames)-1} 个不同位图，{len(pixels)} 字节点阵数据。\n\n")
    f.write("|角色或图表|来源文件 SHA-256|采用的帧引用数|无法可靠分割的栏|\n|---|---|---:|---|\n")
    for name,url,digest,n,skip in rows:f.write(f"|[{name}]({url})|{digest}|{n if name not in ('Calendar Events','Others') else '共用资源'}|{skip or '—'}|\n")
    f.write("\n## 逐角色入库动作与帧数\n\n列出成功提取的组。实际调用见 src/render/lcd.c；调试形态仅提供一般姿势。日历与公共提示共用编号 0。\n\n")
    for who in sorted(set(k[0] for k in sequences)):
        name=next((name for name,index in roster.items() if index==who),"Calendar / Others")
        f.write(f"- {who} {name} — "+", ".join(f"{state} ({len(sequences[who,state])})" for state in states if (who,state) in sequences)+"\n")
    f.write("\n## 参考图有标签、本版未导入的动作\n\n包括未绑定到玩家行为的表情、专属道具和无法可靠分割的多行图。即使有单帧参考，也不能从标签推断触发条件。\n\n")
    for name,unused in excluded:
        f.write(f"- {name} — "+(", ".join(unused) or "列出的动作均有入库帧")+"\n")
print(len(refs),"frames",len(frames)-1,"unique",len(pixels),"bytes")
