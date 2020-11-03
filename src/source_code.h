#pragma once

static const char* common_primitives_h = R"(#pragma once
#include <cstdint>

//!!
typedef bool tt_emit_bool;

//!!
typedef int8_t tt_emit_int8;

//!!
typedef int16_t tt_emit_int16;

//!!
typedef int32_t tt_emit_int32;

//!!
typedef int64_t tt_emit_int64;

//!!
typedef uint8_t tt_emit_uint8;

//!!
typedef uint16_t tt_emit_uint16;

//!!
typedef uint32_t tt_emit_uint32;

//!!
typedef uint64_t tt_emit_uint64;

//!!
typedef float tt_emit_float;

//!!
typedef double tt_emit_double;

//!!
typedef wchar_t tt_emit_wchar_t;

//!!
typedef void tt_emit_void;

//!!
typedef void* tt_emit_voidptr;
)";

static const char* definitions = R"STR(enum class TypeInfoType : uint8_t {
    Primitive,
    Array,
    Indirect,
    Record,
    Enum,
    Function
};

enum class RecordType : uint8_t {
    Struct,
    Class,
    Union
};

enum class RecordAccess : uint8_t {
    Public,
    Protected,
    Private
};

enum class MethodType : uint8_t {
    Normal,
    Virtual,
    Pure
};

enum class IndirectType : uint8_t {
    Pointer,
    LReference,
    RReference
};

struct TypeInfo {
    TypeInfoType type;
    const char* type_name;
    uint32_t type_id;
    uint32_t size;
};

struct TypeInfoArray : public TypeInfo {
    const TypeInfo* underlying;
    int length;
};

struct TypeInfoIndirect : public TypeInfo {
    const TypeInfo* underlying;
    IndirectType indirect_type;
};

struct FunctionParameter {
    const TypeInfo* type_info;
    const char* name;
};

struct TypeInfoFunction : public TypeInfo {
    const char* name;
    const TypeInfo* return_type;
    int parameter_count;
    FunctionParameter* parameters;
    RecordAccess access;
    MethodType method_type;
    int tag_count;
    const char** tags;
};

struct RecordField {
    const TypeInfo* type_info;
    const char* name;
    uint32_t offset;
    RecordAccess access;
    int tag_count;
    const char** tags;
};

struct TypeInfoRecord : public TypeInfo {
    RecordType record_type;
    int parent_count;
    const TypeInfo** parents;
    int field_count;
    RecordField* fields;
    int method_count;
    TypeInfoFunction* methods;
    int tag_count;
    const char** tags;
};

struct TypeInfoEnum : public TypeInfo {
    const TypeInfo* underlying;
    int enum_count;
    const char** enum_names;
    int64_t* enum_values;
    int tag_count;
    const char** tags;
};

template<typename T>
struct Type {
    static const TypeInfo* info() {
        static TypeInfo type;
        type.type = TypeInfoType::Primitive;
        type.type_id = 0;
        type.type_name = "(unindexed)";

        return &type;
    }

    template<typename Result, typename... Args>
    static Result call(T& t, bool& success, const char* name, const Args... args) {
        success = false;
        return Result();
    }
};

template<typename Base, typename Result, typename ...Args>
class Callable {
public:
    Result call(Result(Base::*Member)(Args... args), bool& success, Base& b, Args... args) {
        success = true;
        return (b.*Member)(args...);
    }

    template<typename T>
    Result call(T t, bool& success, Base& b, Args... args) {
        success = false;
        return Result();
    }

    bool valid(Result(Base::*Member)(Args... args)) {
        return true;
    }

    template<typename T>
    bool valid(T t) {
        return false;
    }
};

)STR";

static const char* core_functions = R"(

template<typename T>
static const TypeInfo* type_of() {
    return Type<T>::info();
}


template<typename T>
static const TypeInfo* type_of(const T& t) {
    return type_of<T>();
}

template<typename Result, typename T, typename... Args>
static Result call_method(T& t, bool& success, const char* name, const Args... args) {
    return Type<T>::template call<Result>(t, success, name, args...);
}

template<typename Result, typename T, typename... Args>
static Result call_method(T& t, const char* name, const Args... args) {
    bool success = false;
    return call_method<Result>(t, success, name, args...);
}

// Generic versions
template<typename T>
struct Type<T *> {
    static const TypeInfo* info() {
        static TypeInfoIndirect type;

        type.type = TypeInfoType::Indirect;
        type.type_name = "T *";
        type.type_id = 1;

        type.size = sizeof(T *);
        type.underlying = type_of<T>();
        type.indirect_type = IndirectType::Pointer;

        type_table[type.type_id] = &type;

        return &type;
    }
};

template<typename T>
struct Type<T &> {
    static const TypeInfo* info() {
        static TypeInfoIndirect type;

        type.type = TypeInfoType::Indirect;
        type.type_name = "T &";
        type.type_id = 2;

        type.size = sizeof(T &);
        type.underlying = type_of<T>();
        type.indirect_type = IndirectType::LReference;

        type_table[type.type_id] = &type;

        return &type;
    }
};

template<typename T>
struct Type<T &&> {
    static const TypeInfo* info() {
        static TypeInfoIndirect type;

        type.type = TypeInfoType::Indirect;
        type.type_name = "T &&";
        type.type_id = 3;

        type.size = sizeof(T &&);
        type.underlying = type_of<T>();
        type.indirect_type = IndirectType::RReference;

        type_table[type.type_id] = &type;

        return &type;
    }
};

template<typename T, int size>
struct Type<T [size]> {
    static const TypeInfo* info() {
        static TypeInfoArray type;

        type.type = TypeInfoType::Array;
        type.type_name = "T [size]";
        type.type_id = 4;

        type.size = sizeof(T [size]);
        type.underlying = type_of<T>();
        type.length = size;

        type_table[type.type_id] = &type;

        return &type;
    }
};

)";

static const char* initializer_begin = R"(// This file was generated by TypeTitan
#pragma once
#include "type_titan.h"

namespace tt {

// Call this function to initialize all type info and populate the type table.
// This function is optional, but the type table is only populated once type_of is called.
static void init_type_table() {
)";

static const char* utils_h = R"(// This file was generated by TypeTitan
#pragma once
#include "type_titan.h"

namespace tt {

#define TT_SPAN_HELPER(func_name, span_type, req_type, arr, arr_count)\
static Span<span_type> func_name(const TypeInfo* ti) {\
    if (ti == nullptr) return Span<span_type>(nullptr, 0);\
    \
    if (ti->type != TypeInfoType::req_type) {\
        return Span<span_type>(nullptr, 0);\
    }\
    \
    TypeInfo##req_type* casted_type = (TypeInfo##req_type*)ti;\
    return Span<span_type>(casted_type->arr, casted_type->arr_count);\
}\
\
static Span<span_type> func_name(const TypeInfoArray* ti) {\
    return func_name((const TypeInfo*)ti);\
}\
static Span<span_type> func_name(const TypeInfoIndirect* ti) {\
    return func_name((const TypeInfo*)ti);\
}\
static Span<span_type> func_name(const TypeInfoFunction* ti) {\
    return func_name((const TypeInfo*)ti);\
}\
static Span<span_type> func_name(const TypeInfoRecord* ti) {\
    return func_name((const TypeInfo*)ti);\
}\
static Span<span_type> func_name(const TypeInfoEnum* ti) {\
    return func_name((const TypeInfo*)ti);\
}\
template<typename T>\
static Span<span_type> func_name() {\
    return func_name(type_of<T>());\
}\
template<typename T>\
static Span<span_type> func_name(const T& t) {\
    return func_name(type_of<T>());\
}

template<typename T>
class Span {
    public:
    Span(T* ptr, int size) : ptr(ptr), size(size) { }

    inline T* begin() const {
        return ptr;
    }

inline T* end() const {
        return ptr + size;
    }

    inline int length() const {
        return size;
    }

    inline T* data() const {
        return ptr;
    }

    const T& operator[](int index) {
    return ptr[index];
}

private:
    T* ptr = nullptr;
int size = 0;
};

TT_SPAN_HELPER(get_fields, RecordField, Record, fields, field_count);
TT_SPAN_HELPER(get_parents, const TypeInfo*, Record, parents, parent_count);
TT_SPAN_HELPER(get_methods, TypeInfoFunction, Record, methods, method_count);
TT_SPAN_HELPER(get_parameters, FunctionParameter, Function, parameters, parameter_count);
TT_SPAN_HELPER(get_enum_names, const char*, Enum, enum_names, enum_count);
TT_SPAN_HELPER(get_enum_values, int64_t, Enum, enum_values, enum_count);

#undef TT_SPAN_HELPER

// Getting tags

static Span<const char*> get_tags(const TypeInfo* ti) {
    switch (ti->type) {
        case TypeInfoType::Function:
            return Span<const char*>(((const TypeInfoFunction*)ti)->tags, ((const TypeInfoFunction*)ti)->tag_count);

        case TypeInfoType::Record:
            return Span<const char*>(((const TypeInfoRecord*)ti)->tags, ((const TypeInfoRecord*)ti)->tag_count);

        case TypeInfoType::Enum:
            return Span<const char*>(((const TypeInfoEnum*)ti)->tags, ((const TypeInfoEnum*)ti)->tag_count);
    }

    return Span<const char*>(nullptr, 0);
}

static Span<const char*> get_tags(const RecordField* field) {
    return Span<const char*>(field->tags, field->tag_count);
}

static Span<const char*> get_tags(const TypeInfoArray* ti) {
    return get_tags((const TypeInfo*)ti);
}

static Span<const char*> get_tags(const TypeInfoIndirect* ti) {
    return get_tags((const TypeInfo*)ti);
}

static Span<const char*> get_tags(const TypeInfoFunction* ti) {
    return get_tags((const TypeInfo*)ti);
}

static Span<const char*> get_tags(const TypeInfoRecord* ti) {
    return get_tags((const TypeInfo*)ti);
}

static Span<const char*> get_tags(const TypeInfoEnum* ti) {
    return get_tags((const TypeInfo*)ti);
}

template<typename T>
static Span<const char*> get_tags() {
    return get_tags(type_of<T>());
}

template<typename T>
static Span<const char*> get_tags(const T& t) {
    return get_tags(type_of<T>());
}

// Getting a specific field

static RecordField* get_field(const TypeInfo* ti, const char* field_name) {
    for (auto& field : get_fields(ti)) {
        if (strcmp(field.name, field_name) == 0) {
            return &field;
        }
    }

    return nullptr;
}

static RecordField* get_field(const TypeInfoArray* ti, const char* field_name) {
    return get_field((const TypeInfo*)ti, field_name);
}

static RecordField* get_field(const TypeInfoIndirect* ti, const char* field_name) {
    return get_field((const TypeInfo*)ti, field_name);
}

static RecordField* get_field(const TypeInfoFunction* ti, const char* field_name) {
    return get_field((const TypeInfo*)ti, field_name);
}

static RecordField* get_field(const TypeInfoRecord* ti, const char* field_name) {
    return get_field((const TypeInfo*)ti, field_name);
}

static RecordField* get_field(const TypeInfoEnum* ti, const char* field_name) {
    return get_field((const TypeInfo*)ti, field_name);
}

template<typename T>
static RecordField* get_field(const char* field_name) {
    return get_field(type_of<T>(), field_name);
}

template<typename T>
static RecordField* get_field(const T& t, const char* field_name) {
    return get_field(type_of<T>(), field_name);
}

// Getting a specific method

static TypeInfoFunction* get_method(const TypeInfo* ti, const char* method_name) {
    for (auto& method : get_methods(ti)) {
        if (strcmp(method.name, method_name) == 0) {
            return &method;
        }
    }

    return nullptr;
}

static TypeInfoFunction* get_method(const TypeInfoArray* ti, const char* method_name) {
    return get_method((const TypeInfo*)ti, method_name);
}

static TypeInfoFunction* get_method(const TypeInfoIndirect* ti, const char* method_name) {
    return get_method((const TypeInfo*)ti, method_name);
}

static TypeInfoFunction* get_method(const TypeInfoFunction* ti, const char* method_name) {
    return get_method((const TypeInfo*)ti, method_name);
}

static TypeInfoFunction* get_method(const TypeInfoRecord* ti, const char* method_name) {
    return get_method((const TypeInfo*)ti, method_name);
}

static TypeInfoFunction* get_method(const TypeInfoEnum* ti, const char* method_name) {
    return get_method((const TypeInfo*)ti, method_name);
}

template<typename T>
static TypeInfoFunction* get_method(const char* method_name) {
    return get_method(type_of<T>(), method_name);
}

template<typename T>
static TypeInfoFunction* get_method(const T& t, const char* method_name) {
    return get_method(type_of<T>(), method_name);
}

// Getting a specific parameter

static FunctionParameter* get_parameter(const TypeInfo* ti, const char* parameter_name) {
    for (auto& parameter : get_parameters(ti)) {
        if (strcmp(parameter.name, parameter_name) == 0) {
            return &parameter;
        }
    }

    return nullptr;
}

static FunctionParameter* get_parameter(const TypeInfoArray* ti, const char* parameter_name) {
    return get_parameter((const TypeInfo*)ti, parameter_name);
}

static FunctionParameter* get_parameter(const TypeInfoIndirect* ti, const char* parameter_name) {
    return get_parameter((const TypeInfo*)ti, parameter_name);
}

static FunctionParameter* get_parameter(const TypeInfoFunction* ti, const char* parameter_name) {
    return get_parameter((const TypeInfo*)ti, parameter_name);
}

static FunctionParameter* get_parameter(const FunctionParameter* ti, const char* parameter_name) {
    return get_parameter((const TypeInfo*)ti, parameter_name);
}

static FunctionParameter* get_parameter(const TypeInfoRecord* ti, const char* parameter_name) {
    return get_parameter((const TypeInfo*)ti, parameter_name);
}

static FunctionParameter* get_parameter(const TypeInfoEnum* ti, const char* parameter_name) {
    return get_parameter((const TypeInfo*)ti, parameter_name);
}

template<typename T>
static FunctionParameter* get_parameter(const char* parameter_name) {
    return get_parameter(type_of<T>(), parameter_name);
}

template<typename T>
static FunctionParameter* get_parameter(const T& t, const char* parameter_name) {
    return get_parameter(type_of<T>(), parameter_name);
}

// Checking tags

static bool has_tag(const TypeInfo* ti, const char* tag) {
    for (auto& type_tag : get_tags(ti)) {
        if (strcmp(type_tag, tag) == 0) {
            return true;
        }
    }

    return false;
}

static bool has_tag(const RecordField* field, const char* tag) {
    for (auto& type_tag : get_tags(field)) {
        if (strcmp(type_tag, tag) == 0) {
            return true;
        }
    }

    return false;
}

static bool has_tag(const TypeInfoArray* ti, const char* tag) {
    return has_tag((const TypeInfo*)ti, tag);
}

static bool has_tag(const TypeInfoIndirect* ti, const char* tag) {
    return has_tag((const TypeInfo*)ti, tag);
}

static bool has_tag(const TypeInfoFunction* ti, const char* tag) {
    return has_tag((const TypeInfo*)ti, tag);
}

static bool has_tag(const TypeInfoRecord* ti, const char* tag) {
    return has_tag((const TypeInfo*)ti, tag);
}

static bool has_tag(const TypeInfoEnum* ti, const char* tag) {
    return has_tag((const TypeInfo*)ti, tag);
}

template<typename T>
static bool has_tag(const char* tag) {
    return has_tag(type_of<T>(), tag);
}

template<typename T>
static bool has_tag(const T& t, const char* tag) {
    return has_tag(type_of<T>(), tag);
}

// Getting underlying type

static const TypeInfo* get_underlying(const TypeInfo* ti) {
    switch (ti->type) {
        case TypeInfoType::Array:
            return ((TypeInfoArray*)ti)->underlying;

        case TypeInfoType::Indirect:
            return ((TypeInfoIndirect*)ti)->underlying;

        case TypeInfoType::Enum:
            return ((TypeInfoEnum*)ti)->underlying;
    }

    return nullptr;
}

static const TypeInfo* get_underlying(const TypeInfoArray* ti) {
    return get_underlying((const TypeInfo*)ti);
}

static const TypeInfo* get_underlying(const TypeInfoIndirect* ti) {
    return get_underlying((const TypeInfo*)ti);
}

static const TypeInfo* get_underlying(const TypeInfoFunction* ti) {
    return get_underlying((const TypeInfo*)ti);
}

static const TypeInfo* get_underlying(const TypeInfoRecord* ti) {
    return get_underlying((const TypeInfoRecord*)ti);
}

static const TypeInfo* get_underlying(const TypeInfoEnum* ti) {
    return get_underlying((const TypeInfoEnum*)ti);
}

template<typename T>
static const TypeInfo* get_underlying() {
    return get_underlying(type_of<T>());
}

template<typename T>
static const TypeInfo* get_underlying(const T& t) {
    return get_underlying(type_of<T>());
}

// Getting access

static RecordAccess get_access(const TypeInfo* ti) {
    if (ti->type == TypeInfoType::Function) {
        return ((TypeInfoFunction*)ti)->access;
    }

    return RecordAccess::Private;
}

static RecordAccess get_access(const RecordField* field) {
    return field->access;
}

static RecordAccess get_access(const TypeInfoArray* ti) {
    return get_access((const TypeInfo*)ti);
}

static RecordAccess get_access(const TypeInfoIndirect* ti) {
    return get_access((const TypeInfo*)ti);
}

static RecordAccess get_access(const TypeInfoFunction* ti) {
    return get_access((const TypeInfo*)ti);
}

static RecordAccess get_access(const TypeInfoRecord* ti) {
    return get_access((const TypeInfo*)ti);
}

static RecordAccess get_access(const TypeInfoEnum* ti) {
    return get_access((const TypeInfo*)ti);
}

template<typename T>
static RecordAccess get_access() {
    return get_access(type_of<T>());
}

template<typename T>
static RecordAccess get_access(const T& t) {
    return get_access(type_of<T>());
}

// Casting

template<typename T>
static const T* cast(const TypeInfo* ti) {
    static_assert(false, "invalid cast, T must be derived from TypeInfo");
}

template<>
static const TypeInfoArray* cast(const TypeInfo* ti) {
    return (ti && ti->type == TypeInfoType::Array) ? (const TypeInfoArray*)ti : nullptr;
}

template<>
static const TypeInfoIndirect* cast(const TypeInfo* ti) {
    return (ti && ti->type == TypeInfoType::Indirect) ? (const TypeInfoIndirect*)ti : nullptr;
}

template<>
static const TypeInfoFunction* cast(const TypeInfo* ti) {
    return (ti && ti->type == TypeInfoType::Function) ? (const TypeInfoFunction*)ti : nullptr;
}

template<>
static const TypeInfoRecord* cast(const TypeInfo* ti) {
    return (ti && ti->type == TypeInfoType::Record) ? (const TypeInfoRecord*)ti : nullptr;
}

template<>
static const TypeInfoEnum* cast(const TypeInfo* ti) {
    return (ti && ti->type == TypeInfoType::Enum) ? (const TypeInfoEnum*)ti : nullptr;
}

// Reading/writing arbitrary fields

enum class ReadWritePolicy {
    Type,        // T must match target type
    ExactSize,   // T must be the same size
    FittingSize, // T must be the same size or smaller
    Raw
};

template<typename T, typename U>
static bool write_field(const T& target, const char* field_name, const U& data, ReadWritePolicy policy = ReadWritePolicy::Type) {
    const TypeInfoRecord* tir = cast<TypeInfoRecord>(type_of<T>());

    if (!tir) return false;

    RecordField* field = get_field<T>(field_name);

    if (!field) return false;

    return write_field(target, field, data, policy);
}

template<typename T, typename U>
static bool write_field(const T& target, RecordField* field, const U& data, ReadWritePolicy policy = ReadWritePolicy::Type) {
    const TypeInfo* data_ti = type_of<U>();

    if (!field) return false;

    switch (policy) {
        case ReadWritePolicy::Type:
            if (field->type_info->type_id != data_ti->type_id) return false;
            break;

        case ReadWritePolicy::ExactSize:
            if (field->type_info->size != data_ti->size) return false;
            break;

        case ReadWritePolicy::FittingSize:
            if (field->type_info->size < data_ti->size && data_ti->size > 0) return false;
            break;
    }

    int size = (field->type_info->size < data_ti->size) ? field->type_info->size : data_ti->size;
    memcpy((uint8_t*)&target + field->offset, &data, size);
    return true;
}

template<typename T, typename U>
static bool read_field(const T& target, const char* field_name, const U& data, ReadWritePolicy policy = ReadWritePolicy::Type) {
    const TypeInfoRecord* tir = cast<TypeInfoRecord>(type_of<T>());

    if (!tir) return false;

    RecordField* field = get_field<T>(field_name);

    if (!field) return false;

    return read_field(target, field, data, policy);
}

template<typename T, typename U>
static bool read_field(const T& target, RecordField* field, const U& data, ReadWritePolicy policy = ReadWritePolicy::Type) {
    const TypeInfo* data_ti = type_of<U>();

    if (!field) return false;

    switch (policy) {
        case ReadWritePolicy::Type:
            if (field->type_info->type_id != data_ti->type_id) return false;
            break;

        case ReadWritePolicy::ExactSize:
            if (field->type_info->size != data_ti->size) return false;
            break;

        case ReadWritePolicy::FittingSize:
            if (field->type_info->size < data_ti->size && data_ti->size > 0) return false;
            break;
    }

    int size = (field->type_info->size < data_ti->size) ? field->type_info->size : data_ti->size;
    memcpy((void*)&data, (uint8_t*)&target + field->offset, size);
    return true;
}

}
)";
