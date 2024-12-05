.section .text
.global perform_branch

perform_branch:
    cmp w0, #0         // Compare condition with 0
    beq 1f             // Branch if equal (condition == 0)
    nop                // Never executed if condition == 0

1:
    ret