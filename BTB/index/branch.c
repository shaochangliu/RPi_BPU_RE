// Function containing the conditional branch instruction
void perform_branch(int condition)
{
    __asm__ volatile(
        "cmp %0, #0\n\t" // Compare condition with 0
        "beq 1f\n\t"     // Branch if equal (condition == 0)
        "nop\n\t"        // Never executed if condition == 0
        "add sp, sp, #0x10\n\t"
        "ret\n\t"       // Make sure both directions have the same number of instructions
        "1:\n\t"        // Label 1
        :
        : "r"(condition)
        : "cc", "memory");
}