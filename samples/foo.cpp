#include <iostream>

int f(int x) {
    x *= 2;
    return x;
}

int j(int &x) {
    x += 2;
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

void a_function(int a, int b, int c) {
    return;
}

void a_function_with_call(const int a, const int *b, int &c) {
    a_function(a, b, c);
}

int main () {
    int a, *b, c, *d, e, *g, h;
    int arr[10];

    a_function(a, b, c);
    a_function(d);
    a_function_with_call(e, g, h);

    int x = 1;
    int i = f(x);
    const int y = f(x);
    int z = f(arr[x]);

    a_function(arr[x], arr[i], arr[y]);

    int w = j(arr[x]);

    z++;
    arr[x] = 3;
    x = 2;

    for (i = 0; i < 10; i++) {}

    while (w < 10) {
        w++;
    }

    x = j(arr[x]);

    return 0;
}
