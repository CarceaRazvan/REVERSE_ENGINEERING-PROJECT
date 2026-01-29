import "pe"

rule Detect_IMUL_Instruction
{
    meta:
        description = "Detecteaza instructiuni IMUL (x86)"

    strings:
        $imul_mem = { 0F AF ?? ?? }   // imul reg, [mem]
        $imul_imm = { 6B ?? ?? }      // imul reg, reg, imm8

    condition:
        pe.is_pe and any of them
}