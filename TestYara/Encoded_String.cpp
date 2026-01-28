#include <iostream>

char encoded[] = { 0x1f, 0x0a, 0x1f, 0x0a };

int encodeds()
{
    char key = 0x55;
    for (int i = 0; i < 4; i++) {
        encoded[i] ^= key;
        std::cout << encoded[i];
    }
    return 0;
}
