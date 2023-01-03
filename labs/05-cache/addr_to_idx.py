#!/usr/bin/env python3
import sys


def parse_line(line: str):
    tp, addr_sz, result = line.split(maxsplit=2)
    addr, _ = addr_sz.split(',')

    arr = i = j = None
    addr = int(addr, 16)
    if addr < B_base:
        arr = "A"
        offset = addr - A_base
        n = N
        m = M
    else:
        arr = "B"
        offset = addr - B_base
        n = M
        m = N

    if offset >= 0:
        offset //= 4
        i = offset // n
        j = offset % m

    tag = addr >> 10
    set_idx = (addr - (tag << 10)) >> 5

    if "eviction" in result:
        result = "conflict"
    else:
        result = result.rstrip()

    sys.stdout.write(f"{tp},{tag},{set_idx},{result},")
    if i is not None:
        sys.stdout.write(f"{arr}[{i}][{j}]")
    sys.stdout.write("\n")


A_base = 0x0010d080
B_base = 0x0014d080

N = int(sys.argv[1])
M = int(sys.argv[2])

sys.stdout.write("type,tag,set,result,index\n")
for line in sys.stdin:
    try:
        parse_line(line)
    except Exception:
        sys.stderr.write(line)
