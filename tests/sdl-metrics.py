"""Sample a private Rockbox SDL session using X11 framebuffer changes and CPU ticks."""
import ctypes as c
import os
from pathlib import Path
import statistics
import subprocess
import time

def measure(pid, root, key, quit_plugin, records):
    lib=c.CDLL("libX11.so.6")
    def bind(name,result,*args):
        fn=getattr(lib,name)
        fn.restype=result
        fn.argtypes=list(args)
        return fn
    ptr=c.c_void_p
    display=bind("XOpenDisplay",ptr,c.c_char_p)(None)
    window=int(subprocess.check_output(["xdotool","search","--onlyvisible","--pid",str(pid)]).splitlines()[-1])
    get=bind("XGetImage",ptr,ptr,c.c_ulong,c.c_int,c.c_int,c.c_uint,c.c_uint,c.c_ulong,c.c_int)
    destroy=bind("XDestroyImage",c.c_int,ptr)
    class ImageHeader(c.Structure):
        _fields_=[("w",c.c_int),("h",c.c_int),("offset",c.c_int),("format",c.c_int),("data",ptr),
                  ("byte_order",c.c_int),("unit",c.c_int),("bit_order",c.c_int),("pad",c.c_int),
                  ("depth",c.c_int),("stride",c.c_int)]
    def pixels():
        image=get(display,window,30,48,260,132,c.c_ulong(-1).value,2)
        assert image
        header=ImageHeader.from_address(image)
        data=c.string_at(header.data,header.h*header.stride)
        destroy(image)
        return data
    def transition(button):
        before=pixels()
        start=time.perf_counter()
        subprocess.run(["xdotool","keydown",button],check=True)
        observed=None
        while time.perf_counter()-start<.06:
            if observed is None and pixels()!=before:observed=time.perf_counter()
            time.sleep(.002)
        subprocess.run(["xdotool","keyup",button],check=True)
        if observed is not None:return round(1000*(observed-start),2)
        while pixels()==before:
            if time.perf_counter()-start>4:raise AssertionError(f"No visual change: {button}, colors={len(set(before))}, slot={records(root)[-1][0][2]}")
            time.sleep(.001)
        return round(1000*(time.perf_counter()-start),2)
    def cpu_ticks():
        stat=Path("/proc",str(pid),"stat").read_text().split(") ")[1].split()
        return int(stat[11])+int(stat[12])
    def idle():
        serial=records(root)[-1][0][2];a=cpu_ticks();t=time.monotonic()
        time.sleep(6)
        percent=100*(cpu_ticks()-a)/os.sysconf("SC_CLK_TCK")/(time.monotonic()-t)
        return round(percent,2),records(root)[-1][0][2]-serial
    # Starting in Files after an ordinary, verified plugin exit.
    start_ms=transition("Return")
    time.sleep(2.2)
    home=idle()
    key("space")
    menu=idle()
    latencies=[]
    for _ in range(8):
        latencies.append(transition("Down"))
        time.sleep(.16)
    subprocess.run(["xdotool","keydown","Escape"],check=True)
    time.sleep(1.2)
    subprocess.run(["xdotool","keyup","Escape"],check=True)
    key("Down")
    exit_ms=transition("Return")
    bind("XCloseDisplay",c.c_int,ptr)(display)
    return dict(start_screen_ms=start_ms,exit_screen_ms=exit_ms,menu_navigation_ms=latencies,
                median_navigation_ms=statistics.median(latencies),home_cpu_pct=home[0],
                status_cpu_pct=menu[0],home_writes_6s=home[1],status_writes_6s=menu[1])
