#pragma once
#include "../type_titan/type_titan.h"

void json_serialize_internal(const tt::TypeInfo* ti, void* data, int indentation);

template<typename T>
void json_serialize(const T& t) {
    json_serialize_internal(tt::type_of<T>(), (void*)&t, 0);
}



