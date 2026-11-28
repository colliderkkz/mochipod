"""Run under xvfb-run; exercise the stock Files browser and the real SDL plugin."""
from datetime import datetime, timezone
import json
import os
import pathlib
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import zlib

sim = pathlib.Path(sys.argv[1]).resolve()
out = pathlib.Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
legacy_dir = pathlib.Path(sys.argv[sys.argv.index("--v5")+1]) if "--v5" in sys.argv else None

def key(name):
    subprocess.run(["xdotool", "keydown", name], check=True)
    time.sleep(.12)
    subprocess.run(["xdotool", "keyup", name], check=True)
    time.sleep(.22)

def quit_plugin():
    subprocess.run(["xdotool","keydown","Escape"],check=True)
    time.sleep(1.2)
    subprocess.run(["xdotool","keyup","Escape"],check=True)
    time.sleep(.2)
    key("Return")

def settings():
    key("Escape"); key("Down"); key("Return")

def capture(root, name):
    before = set(root.glob("*.bmp"))
    key("F5")
    images = set(root.glob("*.bmp")) - before
    assert len(images)==1, (name, images)
    # Move each screenshot out of simdisk so second-resolution filenames can be reused.
    shutil.move(images.pop(), out/name)

def records(root):
    found = []
    for path in root.rglob("mochipod-v12.[ab]"):
        data = path.read_bytes()
        if len(data) == 936 and data[4:8] == (12).to_bytes(4,"little") and zlib.crc32(data[:-4]) == int.from_bytes(data[-4:], "little"):
            found.append((struct.unpack("<234I",data), path))
    return sorted(found, key=lambda item:item[0][2])

with tempfile.TemporaryDirectory() as temp:
    root = pathlib.Path(temp)
    shutil.copytree(sim/"simdisk/.rockbox",root/".rockbox")
    # Exercise a new profile in this private copy regardless of local play sessions.
    for local in (root/".rockbox/rocks/games").glob("mochipod-*"):
        if local.suffix in (".a",".b",".cfg"): local.unlink()
    original={}
    if legacy_dir:
        for suffix in ("a","b"):
            path=root/".rockbox/rocks/games"/("mochipod-v3."+suffix)
            data=(legacy_dir/path.name).read_bytes()
            assert len(data)==164 and zlib.crc32(data[:-4])==int.from_bytes(data[-4:],"little")
            path.write_bytes(data)
            original[path]=data
    offer_file=sys.argv[sys.argv.index("--offer")+1] if "--offer" in sys.argv else None
    if offer_file:
        # Private replay checkpoint with a real suitor. Pin process timezone to daylight;
        # neither the computer clock nor a device clock is changed.
        os.environ["TZ"]="UTC%+d"%(datetime.now(timezone.utc).hour-12)
        time.tzset()
        record=bytearray(pathlib.Path(offer_file).read_bytes())
        assert zlib.crc32(record[:-4])==int.from_bytes(record[-4:],"little")
        now=int((datetime.now()-datetime(2000,1,1)).total_seconds()//60)
        struct.pack_into("<I",record,12,now-(2 if "--fidelity" in sys.argv else 0))
        if "--receipt" in sys.argv:
            # Private rc.1 adult fixture, one Hawaii ticket and username AAAAA.
            for field in (10,25,37,38,122):
                struct.pack_into("<I",record,field*4,0)
            struct.pack_into("<I",record,39*4,1)
            record[504:600]=bytes(96)
        struct.pack_into("<I",record,len(record)-4,zlib.crc32(record[:-4]))
        v=struct.unpack_from("<I",record,4)[0]
        assert v in (9,10,11)
        offer_path=root/f".rockbox/rocks/games/mochipod-v{v}.a"
        offer_path.write_bytes(record)
    night_run="--night" in sys.argv
    if night_run:
        # An immutable host backup supplied explicitly by the tester. Only the private
        # test copy's timestamp is aligned to the simulator's process-local timezone.
        origin=pathlib.Path(sys.argv[sys.argv.index("--night")+1])
        original_night=origin.read_bytes()
        data=bytearray(original_night)
        assert struct.unpack_from("<I",data,4)[0]==8 and zlib.crc32(data[:-4])==int.from_bytes(data[-4:],"little")
        os.environ["TZ"]="UTC%+d"%(datetime.now(timezone.utc).hour-21)
        time.tzset()
        stamp=int((datetime.now()-datetime(2000,1,1)).total_seconds()//60)
        struct.pack_into("<I",data,12,stamp)
        struct.pack_into("<I",data,len(data)-4,zlib.crc32(data[:-4]))
        old_night=root/".rockbox/rocks/games/mochipod-v8.a"
        old_night.write_bytes(data)
    growth_run="--growth" in sys.argv
    def rtc_minute():
        return int((datetime.now()-datetime(2000,1,1)).total_seconds()//60)
    long_v7="--long-v7" in sys.argv
    v8_run="--v8" in sys.argv
    if v8_run:
        # Clock-fix wire format. Derive from the immutable v7 fixture; carry a pending clock review.
        v8=root/".rockbox/rocks/games/mochipod-v8.a"
        old8=bytearray((pathlib.Path(__file__).parent/"fixtures/v7-baby.a").read_bytes()[:-4])
        struct.pack_into("<I",old8,4,8)
        struct.pack_into("<I",old8,12,rtc_minute()-4)
        old8+=struct.pack("<I",3)
        old8+=struct.pack("<I",zlib.crc32(old8))
        v8.write_bytes(old8)
    if long_v7:
        v7=root/".rockbox/rocks/games/mochipod-v7.a"
        older=bytearray((pathlib.Path(__file__).parent/"fixtures/v7-baby.a").read_bytes())
        struct.pack_into("<I",older,12,rtc_minute()-40*1440)
        struct.pack_into("<I",older,196,zlib.crc32(older[:196]))
        v7.write_bytes(older)
    if growth_run:
        v6=root/".rockbox/rocks/games/mochipod-v6.a"
        older=bytearray((pathlib.Path(__file__).parent/"fixtures/v6-child.a").read_bytes())
        struct.pack_into("<I",older,12,rtc_minute()-2)
        struct.pack_into("<I",older,176,zlib.crc32(older[:176]))
        v6.write_bytes(older)
    (root/"play").mkdir()
    shutil.copy(root/".rockbox/rocks/games/mochipod.rock", root/"play/01.rock")
    replay="--family-replay" in sys.argv
    if replay:
        shutil.copy(sys.argv[sys.argv.index("--family-replay")+1],root/"play/01.rock")
    (root/".rockbox/config.cfg").write_text("start in screen: files\nstart directory: /play\nshow files: all\n")
    with (out/"simulator.log").open("w") as log:
        def boot():
            emulator = subprocess.Popen([sim/"rockboxui","--root",root,"--nobackground"],
                  cwd=sim,stdout=log,stderr=log,env={**os.environ,"SDL_AUDIODRIVER":"dummy","SDL_VIDEODRIVER":"x11"})
            time.sleep(3)
            window = subprocess.check_output(["xdotool","search","--onlyvisible","--pid",str(emulator.pid)],text=True).splitlines()[-1]
            subprocess.run(["xdotool","windowfocus",window],check=True)
            return emulator
        proc=boot()
        try:
            if "--migration-fail" in sys.argv:
                assert offer_file and v in (10,11)
                staging=root/".rockbox/rocks/games/mochipod-v12.tmp"
                staging.mkdir()
                for _ in range(2):
                    key("Return");time.sleep(2.5)
                assert not records(root) and offer_path.read_bytes()==record
                proc.terminate();proc.wait(timeout=5)
                staging.rmdir()
                proc=boot()
                print("PASS interrupted first old-slot migration preserves original; retrying ordinary startup")
            key("Return")
            time.sleep(2)
            if not records(root) or (offer_file and len(records(root))==1):
                key("Return")
                time.sleep(2)
            if not records(root) and not v8_run:
                key("F5")
                time.sleep(1)
                for shot in root.rglob("*.bmp"):
                    if shot.parent == root: shutil.copy(shot,out/"startup.bmp")
            assert v8_run or records(root), "Plugin did not create first slot"
            if "--receipt" in sys.argv:
                for _ in range(3):key("Down")
                key("Return");key("Down");key("Return");key("Return")
                capture(root,"flight.bmp")
                state=records(root)[-1][0]
                assert state[39]==0 and not state[122] & (1<<10)
                time.sleep(4.1)
                key("Escape")
                for _ in range(3):key("Down")
                key("Return")
                for _ in range(10):key("Down")
                capture(root,"return-code.bmp")
                key("Escape");key("Up");key("Return")
                # Independent AAAAA/Hawaii code, V3 generator, canonical flag zero.
                for digit in "5917209366":
                    steps=int(digit)
                    for _ in range(min(steps,10-steps)):key("Down" if steps<=5 else "Up")
                    key("Return")
                capture(root,"souvenir-claim.bmp")
                key("Return")
                assert records(root)[-1][0][122] & (1<<10)
                quit_plugin();key("Return");time.sleep(2)
                assert records(root)[-1][0][122] & (1<<10) and offer_path.read_bytes()==record
                print("PASS ordinary SDL Hawaii ticket, deferred souvenir, local receipt, manual password claim and restart")
                sys.exit(0)
            if "--identity" in sys.argv:
                original_fields=struct.unpack_from("<50I",record)
                for _ in range(2):
                    got=records(root)[-1][0]
                    for field in (16,26,27,40,41,44,45,46,47,48,49):
                        assert got[field]==original_fields[field],field
                    quit_plugin();key("Return");time.sleep(2)
                assert offer_path.read_bytes()==record
                name="".join(chr(65+got[26]//26**i%26) for i in range(5))
                print(f"PASS old-slot identity through two starts: {name}, form {got[40]}, generation {got[41]}, stage {got[16]}, hold {got[49]}")
                sys.exit(0)
            if "--fidelity" in sys.argv:
                # Synthetic rc.1 co-resident family, 59 minutes after birth.
                before=records(root)[-1][0]
                assert before[40:42]==(20,1) and before[50]==2 and before[232]==1
                capture(root,"sick-baby.bmp")
                for _ in range(6):key("Down")
                key("Return")
                assert records(root)[-1][0][232]==0 and records(root)[-1][0][10]==before[10]
                capture(root,"baby-medicine.bmp")
                time.sleep(2)
                for _ in range(3):key("Up")
                key("Return");key("Down");key("Return");key("Return")
                time.sleep(2.1);capture(root,"pyonkotchi-drawing.bmp")
                current=records(root)[-1][1].read_bytes()
                pencil_offset=504+50
                assert current[pencil_offset]==0 and record[pencil_offset]==1
                assert offer_path.read_bytes()==record
                print("PASS ordinary SDL migration, co-resident baby treatment and one-use Pyonkotchi pencil via menus")
                sys.exit(0)
            if night_run:
                time.sleep(2);capture(root,"asleep.bmp")
                assert records(root)[-1][0][40]==4
                for _ in range(5):key("Down")
                was_dark=records(root)[-1][0][15]
                key("Return");time.sleep(2)
                if was_dark:
                    assert records(root)[-1][0][15]==0
                    key("Return");time.sleep(2)
                capture(root,"lights-off.bmp");assert records(root)[-1][0][15]==1
                quit_plugin()
                key("Return");time.sleep(2);capture(root,"night-reopen.bmp")
                assert records(root)[-1][0][15]==1
                quit_plugin()
                saved_seq=records(root)[-1][0][2]
                proc.terminate();proc.wait(timeout=5)
                # Advance just the private simulator RTC into the next morning by 11 hours.
                offset=int(os.environ["TZ"][3:])-11
                os.environ["TZ"]="UTC%+d"%offset;time.tzset()
                proc=boot()
                for _ in range(3):
                    key("Return");time.sleep(1.8)
                    if records(root)[-1][0][2]>saved_seq: break
                woke=records(root)[-1][0]
                assert woke[2]>saved_seq and woke[15]==0 and woke[3]>stamp+640 and woke[40]==4
                capture(root,"morning.bmp")
                assert woke[26]==struct.unpack_from("<I",data,26*4)[0] and woke[167+4]>>16==1
                assert old_night.read_bytes()==data and origin.read_bytes()==original_night
                print("PASS SDL IEBBB backup, Mizutamatchi at night, lights off persisted, morning wake after offline night, unchanged v8 inputs")
                sys.exit(0)
            if offer_file:
                time.sleep(1)
                key("Return")
                capture(root,"live-wedding.bmp")
                assert records(root)[-1][0][50]==2
                time.sleep(3.2);capture(root,"live-birth.bmp")
                time.sleep(3.4);quit_plugin()
                key("Return");time.sleep(2)
                assert records(root)[-1][0][50:54]==records(root)[-2][0][50:54]
                # Earn a secret object through public menus on the standard release plugin.
                for _ in range(3):key("Down")
                key("Return");key("Down");key("Down");key("Return")
                for _ in range(4):key("Down")
                for letter in "ACBBBACC":
                    for _ in range(ord(letter)-ord("A")):key("Down")
                    key("Return")
                capture(root,"shop-claim.bmp");key("Return")
                assert records(root)[-1][0][121]&2
                if v<12: assert offer_path.read_bytes()==record
                quit_plugin();key("Return");time.sleep(2)
                for _ in range(8):key("Down")
                key("Return")
                for _ in range(4):key("Down")
                key("Return");capture(root,"journal.bmp")
                key("Return");capture(root,"journal-detail.bmp")
                key("Escape");key("Down");key("Return");capture(root,"collection.bmp")
                quit_plugin()
                if "--metrics" in sys.argv:
                    import runpy
                    metrics=runpy.run_path(str(pathlib.Path(__file__).with_name("sdl-metrics.py")))["measure"]
                    result=metrics(proc.pid,root,key,quit_plugin,records)
                    (out/"metrics.json").write_text(json.dumps(result,indent=2)+"\n")
                print("PASS stock SDL plugin, physical-key proposal acceptance, immediate committed birth, reopen and shop-code claim")
                sys.exit(0)
            if v8_run:
                original8=struct.unpack("<51I",old8)
                assert original8[49]==3 and not records(root)
                capture(root,"v8-pending-review.bmp")
                key("Return");key("Return")
                current=records(root)[-1][0]
                assert current[49]==0 and current[4:49]==original8[4:49]
                quit_plugin();key("Return");time.sleep(2);quit_plugin()
                assert len(records(root))==2 and v8.read_bytes()==old8
                print("PASS SDL v8 clock hold to v12, safe rebase, double slot, restart and unchanged input fixture")
                sys.exit(0)
            if long_v7:
                held=records(root)[-1][0]
                old=struct.unpack("<50I",older)
                assert held[49] and held[3:49]==old[3:49]
                key("space");key("Return")
                settled=records(root)[-1][0]
                assert settled[16]==5 and not settled[49] and settled[2]==held[2]+1
                quit_plugin()
                key("Return");time.sleep(1)
                assert records(root)[-1][0][4:49]==settled[4:49]
                assert v7.read_bytes()==older
                print("PASS SDL v7 migration, confirmed genuine 40-day absence, normal death, no repeated settlement")
                sys.exit(0)
            if replay:
                timeline=[]
                for gen in range(1,5):
                    for scene in (["history"] if gen==4 else ["offer","wedding","birth","departure"]):
                        marker=root/f"replay-{gen}-{scene}"
                        for _ in range(100):
                            if marker.exists():break
                            time.sleep(.05)
                        assert marker.exists(), (gen,scene)
                        state=records(root)[-1][0]
                        assert state[41]==gen and state[16]==4
                        timeline.append(dict(generation=gen, scene=scene, serial=state[2],
                            minute_since_2000=state[3], pet_name=state[26], form=state[40],
                            mate=state[51], baby_sex=state[52], birth_minute=state[53],
                            phase=state[50], parent_count=state[55]))
                        if gen==1 and scene=="offer":
                            shutil.copyfile(records(root)[-1][1],out/"route-offer.a")
                        capture(root,f"gen-{gen}-{scene}.bmp")
                        key("Return")
                assert state[55]==3 and state[56]==3 and state[64]==2 and state[72]==1
                (out/"route.json").write_text(json.dumps(timeline,indent=2)+"\n")
                print("PASS Rockbox SDL egg through fourth-generation adult, 3 weddings/births/departures, 13 verified disk checkpoints")
                sys.exit(0)
            if growth_run:
                teenage=records(root)[-1][0]
                assert teenage[16]==3 and 8<=teenage[40]<=12
                capture(root,"growth-teen.bmp")
                time.sleep(2.8)
                capture(root,"teen-home.bmp")
                quit_plugin()
                fields,newest=records(root)[-1]
                assert fields[40]==teenage[40] and fields[26]==teenage[26]
                # Advance only the sandbox clock/age to one minute before adulthood.
                data=bytearray(newest.read_bytes())
                struct.pack_into("<II",data,12,rtc_minute()-2,fields[44]+4319)
                struct.pack_into("<I",data,len(data)-4,zlib.crc32(data[:-4]))
                newest.write_bytes(data)
                key("Return"); time.sleep(2)
                capture(root,"growth-adult.bmp")
                time.sleep(2.8)
                capture(root,"adult-home.bmp")
                key("space")
                for i in range(6): key("Down")
                capture(root,"adult-id.bmp")
                quit_plugin()
                adult=records(root)[-1][0]
                assert adult[16]==4 and 18<=adult[40]<=32
                assert adult[46]>>24==adult[40] and (adult[46]>>16)&255==teenage[40]
                key("Return");time.sleep(2);quit_plugin()
                assert records(root)[-1][0][40:47]==adult[40:47] and v6.read_bytes()==older
                print("PASS SDL v6 child -> teen -> adult, two growth displays, history, restart, old slot intact")
                sys.exit(0)
            if legacy_dir:
                latest=max(original.values(),key=lambda data:int.from_bytes(data[8:12],"little"))
                old=struct.unpack("<41I",latest)
                held=records(root)[-1][0]
                assert held[49] and held[3:40]==old[3:40] and held[40:42]==(0,0)
                capture(root,"clock-hold.bmp")
                # Abrupt power loss after the hold commit. A torn staging file
                # from an interrupted decision must never be selected on boot.
                proc.kill();proc.wait()
                staging=root/".rockbox/rocks/games/mochipod-v12.tmp"
                staging.write_bytes(b"MPV3")
                proc=boot()
                # The Files browser may start in /play or one level above it.
                for attempt in range(2):
                    key("Return");time.sleep(.8)
                    capture(root,"after-power-loss.bmp")
                    if (out/"after-power-loss.bmp").read_bytes()[-320*2*40:]==(out/"clock-hold.bmp").read_bytes()[-320*2*40:]:
                        break
                else:
                    raise AssertionError("Clock review did not return after power loss")
                assert records(root)[-1][0]==held
                staging.unlink()
                key("Return")
                capture(root,"clock-confirm.bmp")
                key("Escape")  # cancel the decision
                assert records(root)[-1][0]==held
                staging.mkdir()  # force an open failure only on this sandbox
                key("Return");key("Return");time.sleep(5)
                assert records(root)[-1][0]==held
                staging.rmdir()
                key("Escape");time.sleep(1)
                key("Return");time.sleep(1)
                key("Return");key("Return")
                saved=records(root)[-1][0]
                assert saved[49]==0 and saved[2]==held[2]+1
                assert saved[4:49]==held[4:49] and abs(saved[3]-rtc_minute())<=1
                quit_plugin()
                key("Return"); time.sleep(2)
                quit_plugin()
                assert len(records(root))==2 and not records(root)[-1][0][49]
                assert all(path.read_bytes()==data for path,data in original.items())
                print("PASS SDL real v5 clock hold, cancel, restart, failed commit, retry, rebase, v12 double slot and unchanged originals")
                sys.exit(0)
            # The real device RTC, including its seconds field, drives this interval.
            for _ in range(7): key("Return")
            started=time.monotonic()
            capture(root,"hatch-egg.bmp")
            time.sleep(max(0,57.2-(time.monotonic()-started)))
            capture(root,"hatch-crack.bmp")
            time.sleep(max(0,61.5-(time.monotonic()-started)))
            capture(root,"hatch-reveal.bmp")
            quit_plugin()
            fields=records(root)[-1][0]
            assert fields[16]==1 and fields[22]==13 and fields[40] in (2,3),fields
            assert fields[40]==2+fields[27] and fields[41]==1
            key("Return"); time.sleep(2)
            key("Return") # acknowledge the saved reveal
            capture(root,"hatch-name.bmp")
            for _ in range(5): key("Return")
            capture(root,"hatch-baby.bmp")
            key("Down"); key("Return"); key("Return"); key("Return")
            capture(root,"sim-feed.bmp")
            quit_plugin()
            saved=records(root)[-1][0]
            assert saved[6]==1 and saved[8]==6,saved
            key("Return")
            time.sleep(2)
            key("space")
            capture(root,"status.bmp")
            key("Escape")
            if "--display" in sys.argv:
                settings()
                key("Down"); key("Return")
                for page in range(4):
                    capture(root,f"display-{page}.bmp")
                    if page<3: key("Down" if page==0 else "Return")
                key("Escape")
                for i in range(10):
                    capture(root,f"icon-{i}.bmp")
                    key("Down")
            settings()
            key("Return"); key("Down"); key("Return")
            marker=root/".rockbox/rocks/games/mochipod-classic.cfg"
            assert marker.is_file(),"Classic preference was not written"
            quit_plugin()
            restored=records(root)[-1][0]
            assert restored[6]==1 and restored[8]==6 and restored[16]==1,restored
            # Keep feeding through the original A/B/C keys after a fresh plugin load.
            records(root)[-1][1].write_bytes(b"MP")
            key("Return")
            time.sleep(2)
            key("Left"); key("Left"); key("Return"); key("Return"); key("Return")
            quit_plugin()
            assert records(root)[-1][0][8]==7,"Classic controls or recovery failed"
            key("Return"); time.sleep(2)
            key("Escape"); key("Left"); key("Return"); key("Return")
            key("Left"); key("Return")
            assert not marker.exists(),"Native preference was not restored"
            quit_plugin()
            print("PASS SDL native wheel, status, long MENU, meal, reopen, mode persistence, Classic meal, truncated-slot recovery")
        finally:
            proc.terminate()
            try: proc.wait(timeout=4)
            except subprocess.TimeoutExpired: proc.kill(); proc.wait()




