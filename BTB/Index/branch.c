// Function containing the unconditional branch instruction
void perform_branch()
{
    __asm__ volatile(
        "b 1f\n\t"     // Unconditional branch to label 1
        "nop\n\t"      // This instruction will be skipped
        "1:\n\t"       // Label 1
        :
        :
        : "cc", "memory");
}