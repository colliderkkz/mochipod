"""Package only the ARM plugin after checking the Rockbox 4.0 ipod6g header."""
import argparse
import hashlib
from pathlib import Path
import struct
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED

parser=argparse.ArgumentParser()
parser.add_argument("plugin", type=Path)
parser.add_argument("out", type=Path)
args=parser.parse_args()
data=args.plugin.read_bytes()
magic,target,api,load,end,entry,api_ptr,api_size=struct.unpack_from("<4sHHIIIII",data)
if (magic,target,api,load,api_size)!=(b"KcoR",71,273,0x0bdfc000,1644):
    raise ValueError("Unexpected Rockbox 4.0 ipod6g plugin header")
if not (load<entry<load+len(data)<=end and load<=api_ptr<end):
    raise ValueError("Invalid plugin address range")
args.out.mkdir(parents=True,exist_ok=True)
(args.out/"mochipod.rock").write_bytes(data)
archive=args.out/"mochipod-ipod6g-rockbox4.zip"
path=".rockbox/rocks/games/mochipod.rock"
with ZipFile(archive,"w",ZIP_DEFLATED) as z:
    entry=ZipInfo(path,(2006,1,1,0,0,0))
    entry.create_system=3
    entry.external_attr=0o100644<<16
    z.writestr(entry,data,compress_type=ZIP_DEFLATED,compresslevel=9)
with ZipFile(archive) as z:
    assert z.namelist()==[path] and z.read(path)==data
print(f"{len(data)} bytes, sha256 {hashlib.sha256(data).hexdigest()}")

