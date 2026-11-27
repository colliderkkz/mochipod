"""Compile and execute host suites. Set SANITIZE=1 with a supported GCC."""
from pathlib import Path
import os
import subprocess

root=Path(__file__).resolve().parents[1]
build=root/"build"
build.mkdir(exist_ok=True)
common=["src/core/sound.c","src/core/shop.c","src/core/family.c","src/core/rtc_guard.c","src/core/growth.c","src/core/characters.c","src/core/game.c","src/core/ui.c","src/core/play.c"]
suites={
    "commerce": ["src/storage/save.c"],
    "sleep": ["src/storage/save.c"],
    "week": ["src/storage/save.c"],
    "family": ["src/storage/save.c"],
    "rtc": ["src/storage/save.c"],
    "growth": ["src/storage/save.c"],
    "hatching": ["src/storage/save.c"],
    "core": ["src/storage/save.c","src/render/lcd.c"],
    "wheel": ["src/input/wheel.c"],
    "prompts": ["-DMOCHI_LAYOUT_TEST","src/input/wheel.c","src/render/lcd.c"],
    "display": ["-DMOCHI_LAYOUT_TEST","src/render/lcd.c"],
}
flags=["-std=c99","-O2","-Wall","-Wextra","-Werror","-pedantic"]
if os.environ.get("SANITIZE")=="1":
    flags+=["-O1","-fsanitize=address,undefined","-fno-sanitize-recover=all","-fno-omit-frame-pointer"]
for name, extra in suites.items():
    output=build/(f"host-{name}"+(".exe" if os.name=="nt" else ""))
    subprocess.run([os.environ.get("CC","gcc"),*flags,f"tests/{name}.c",*common,*extra,"-o",output],cwd=root,check=True)
    subprocess.run([output],cwd=root,check=True,timeout=45)
    if name=="week": subprocess.run([output,"30"],cwd=root,check=True,timeout=45)

