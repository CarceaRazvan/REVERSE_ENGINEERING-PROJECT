rule XOR_Encoded_String {
    meta:
        description = "Detects XOR encoded payload"
        severity = "high"

    strings:
        $xor = { 1F 0A 1F 0A }

    condition:
        $xor
}