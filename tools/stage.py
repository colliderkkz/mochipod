"""Register MochiPod in a dedicated Rockbox 4.0 source checkout."""
import pathlib
import shutil
import sys

root = pathlib.Path(__file__).resolve().parents[1]
plugins = pathlib.Path(sys.argv[1]).resolve() / "apps/plugins"
target = plugins / "mochipod_src"
target.mkdir(exist_ok=True)
shutil.copytree(root / "src", target, dirs_exist_ok=True)
modules = ["core/shop", "core/family", "core/rtc_guard", "core/growth", "core/characters", "core/game", "core/play", "core/ui","core/sound", "storage/save", "input/wheel", "render/lcd", "platform/rockbox"]
# A single plugin translation unit uses the standard single-file Rockbox rule.
(plugins / "mochipod.c").write_text("".join(
    f'#include "mochipod_src/{name}.c"\n' for name in modules))
for filename, line in [("SOURCES", "mochipod.c"), ("CATEGORIES", "mochipod,games")]:
    path = plugins / filename
    content = path.read_text()
    if line not in content.splitlines():
        entry = f"#if defined(IPOD_6G)\n{line}\n#endif" if filename=="SOURCES" else line
        path.write_text(content.rstrip()+"\n"+entry+"\n")

