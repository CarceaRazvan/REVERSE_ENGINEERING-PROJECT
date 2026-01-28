rule admin_tool {

    meta:
        description = "A simple proof-of-concept to show YARA rules"
        author = "Carcea Razvan"

    strings:
        $cmd1 = "whoami"
        $cmd2 = "ipconfig"
        $cmd3 = "net user"
        $log  = "output.log"
        
    condition:
        2 of ($cmd*) and $log

}