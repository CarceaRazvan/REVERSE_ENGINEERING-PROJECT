rule Simple_C2_Beacon {
    strings:
        $url = /http(s)?:\/\/.*\/api/
        $beacon = "beacon"

    condition:
        $url and $beacon
}
