#pragma once
#include <cstdint>

//!! Tags=Very,Cool
enum class CoolFactor {
    Lame,
    Meh,
    Cool,
    Awesome
};

//!!
struct POD {
    int i;
    float x;
    void**** p;
};

//!!
struct Bar {
    double bro = 1.2345;

    //!! Tags=DoNotPrint,DoNotSerialize
    uint32_t secret_code = 12345;
};

//!!
union Xyz {
    Xyz() {}

    float x;
    Bar y;
    int z;

    struct {
        bool q;
        char w;
    };
};

namespace special {

//!!
class Foo {
    int nooo = -1;
};

}

//!!
struct Recursive {
    Recursive* r;
};

//!!
class Foo {
public:
    int i = 5;
    CoolFactor factor = CoolFactor::Awesome;
    Bar bar;

    //!! Tags=DoNotPrint
    void* ptr = nullptr;
    const char* msg = "Hello, world!";

    int ints[4] = {
        2, 5, 9, -3
    };

    int two_d[2][2][2] = {
        { 
            { 1, 2 },
            { 3, 4 }
        },
        {
            { 5, 6 },
            { 7, 8 }
        }
    };

    Xyz xyz;
    char sym = '!';
};

//!!
class Base {
public:
    int id = 1;
    virtual int get_nice_number() = 0;
};

//!!
class Base2 {
public:
    float x;
};

//!!
class Base3 {
public:
    float x;
};

//!!
class Derived : public Base, public Base2, public Base3 {
public:
    Derived() = default;
    Derived(int i) {

    }

    const char* msg = "Hey!";
    const char* msg2 = "How're you?";

    virtual int get_nice_number() override {
        return 2;
    }
};

//!!
template<typename T>
class Templated {
public:
    T& get_t() {
        return t;
    }

protected:
    T t;
};

//!!
class FuncTown {
public:
    int get_int() { return 5; }
    float multiply(float a, float b) { return a * b; }
    Foo new_foo(CoolFactor factor, bool is_cool = true) { return Foo(); }
};

namespace creator {

//!!
static Foo create_foo(CoolFactor factor, const char* msg = "Hello, world!") {
    Foo foo;
    foo.factor = factor;
    foo.msg = msg;

    return foo;
}

}
