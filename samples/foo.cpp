#include <cstdlib>

void a_function(const int a, const int *b) {
    /* NO CALL, DOES NOT RETURN */
}

void a_function_with_call(const int a, const int *b) {
    a_function(a, b);
}

int main () {
    int a, *b, c;

    a = 10;
    b = (int*)malloc(a * sizeof(int));

    a_function_with_call(a, b);

    return 0;
}
