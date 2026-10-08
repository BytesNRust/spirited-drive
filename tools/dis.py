#!/usr/bin/env python3
"""Raw x86 disassembly of NFS_WIN.EXE at a virtual address, for code Ghidra
did not make into a function (jump-table targets, labels reached through
function pointers).
    dis.py ADDR [COUNT]   COUNT instructions (default 60), stops after RET"""
import sys
from pathlib import Path

import capstone

sys.path.insert(0, str(Path(__file__).resolve().parent))
import peek  # noqa: E402


def main():
    addr = int(sys.argv[1], 16)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 60
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    code = peek.read(addr, count * 15)
    for n, ins in enumerate(md.disasm(code, addr)):
        print(f"{ins.address:08x}  {ins.mnemonic} {ins.op_str}")
        if n + 1 >= count or ins.mnemonic == "ret":
            break


if __name__ == "__main__":
    main()
