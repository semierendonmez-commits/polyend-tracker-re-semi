#!/usr/bin/env python3
"""Fail the build when a static constructor touches SDRAM.

Static constructors (_GLOBAL__sub_I_*) run from __libc_init_array before
main(), before boardInit() has started the SDRAM controller: a store to
0x70000000-0x7FFFFFFF then bus-faults and the Tracker stays black at power
on (2.2.3: SdStreamReader's .sdram_bss bindings had default member values).
Objects in OT_SDRAM_BSS must be trivially constructible and set up at run
time. This scans every _GLOBAL__sub_I_* function of the linked ELF for an
SDRAM address in its literal pool or a movw / movt pair.

Usage: check_ctors.py <objdump> <image.elf>
"""
import re
import subprocess
import sys

SDRAM_LO, SDRAM_HI = 0x70000000, 0x80000000


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    objdump, elf = sys.argv[1], sys.argv[2]
    out = subprocess.run([objdump, "-d", "--no-show-raw-insn", elf], capture_output=True, text=True, check=True).stdout
    bad = []
    fn = None
    for line in out.splitlines():
        m = re.match(r"^[0-9a-f]+ <(.+)>:$", line)
        if m:
            fn = m.group(1) if m.group(1).startswith("_GLOBAL__sub_I_") else None
            continue
        if not fn:
            continue
        w = re.search(r"\.word\s+0x([0-9a-f]+)", line)
        if w and SDRAM_LO <= int(w.group(1), 16) < SDRAM_HI:
            bad.append((fn, line.strip()))
        t = re.search(r"\bmovt\s+\w+,\s*#(\d+)", line)
        if t and SDRAM_LO <= (int(t.group(1)) << 16) < SDRAM_HI:
            bad.append((fn, line.strip()))
    if bad:
        print("check_ctors: static constructors that reach SDRAM before it is initialised:")
        for f, l in bad:
            print(f"  {f}: {l}")
        print("  -> give the OT_SDRAM_BSS object a trivial type and set it up at run time")
        return 1
    print("check_ctors: ok (no static constructor touches SDRAM)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
