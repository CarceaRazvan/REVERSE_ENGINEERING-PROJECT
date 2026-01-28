#include <iostream>
#include <fstream>
#include <cstdlib>

int admin()
{
    std::ofstream log("output.log");

    log << "Starting command execution...\n";
    system("whoami");
    system("ipconfig");
    system("net user");

    log << "Execution finished\n";
    log.close();

    return 0;
}
