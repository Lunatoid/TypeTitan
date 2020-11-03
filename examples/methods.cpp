
class Foo {
public:
    float multiply(float a, float b) {
        return a * b;
    }

    int multiply(int a, int b) {
        return a * b;
    }

    void say_hello() {
        printf("Hello!\n");
    }
};

void call_methods() {
    Foo foo;

    float f = tt::call_method<float>(foo, "multiply", 5.5f, 10.1f);
    int i = tt::call_method<int>(foo, "multiply", 5, 10);

    bool success = false;
    double non_existent = tt::call_method<double>(foo, "i_do_not_exist", 1, 2, 3, 4, 5);

    if (!success) {
        printf("i_do_not_exist failed to be called\n");
    }

    tt::call_method<void>(foo, "say_hello");

    printf("-----\nf: %.2f\ni: %d\n", f, i);
}
