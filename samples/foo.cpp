#include <iostream>

void foo(int x) {
    std::cout << "HELLO\n";
    std::cout << "WORLD\n";
    std::cout << "x: " << x;
}

int main () {
    int x = 10;

    if (x == 10) {
        std::cout << "HELLO\n";
    } else {
        std::cout << "WORKD\n";
    }

    foo(x);

    return 0;
}
