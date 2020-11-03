#pragma once
#include "type_titan.h"

void print_internal(const tt::TypeInfo* ti, void* data, int indentation);

template<typename T>
void print(const T& t) {
    print_internal(tt::type_of<T>(), (void*)&t, 0);
}
