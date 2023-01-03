#!/usr/bin/env python3
import sys

A_base = 0x0010d080
B_base = 0x0014d080

N = int(sys.argv[1])
M = int(sys.argv[2])
group = int(sys.argv[3])


def to_csv(base, n, m):
    for i in range(n):
        curr = []
        for j in range(0, m, group):
            addr = base + (i * n + j) * 4
            set_idx = (addr - (addr >> 10 << 10)) >> 5
            curr.append(str(set_idx))

        print(",".join(curr))


to_csv(A_base, N, M)
print()
to_csv(B_base, M, N)
