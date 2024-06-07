#include <iostream>

int f(int x) {
    x *= 2;
    return x;
}

void a_function(const int a, const int *b,  int &c) {
    std::cout << "a: " << a;
    std::cout << "b: " << b;
    std::cout << "c: " << c;
}

int a_function(int *b) {
    std::cout << "B: " << b;
    return 10;
}

void a_function_with_call(const int a, const int *b, int &c) {
    a_function(a, b, c);
}

int main () {
    int a;
    int *b;
    int c;

    a_function(a/2, b, c);
    a_function_with_call(a, b, c);
    int d = a_function(b) + a_function(b);
    int e = a_function(b);
    a_function(a, b, c);
    a_function_with_call(a+1, b, c);

    int x = 1;
    const int y = f(x);

    return 0;
}
