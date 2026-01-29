#include <iostream>
#include <string>
#include <thread>
#include <chrono>

int endpoint()
{
    // Simulated C2 endpoint (educational)
    const std::string c2_url     = "https://example-not-real-c2.com/api";
    const std::string beacon_msg = "beacon";

    while (true) {
        std::cout << "[INFO] Sending " << beacon_msg << " to " << c2_url << std::endl;

        // simulate sleep interval like real malware
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    return 0;
}
