#include <stdio.h>

int main() {
    int num = 255;
    char buffer[33];

    snprintf(buffer, sizeof(buffer), "%d", num);
    printf("Chaîne : %s\n", buffer);

    return 0;
}
