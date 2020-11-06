
# Kinds of `TypeInfo`
There are several different types of `TypeInfo`:

  - [`TypeInfo`](#TypeInfo)
  - [`TypeInfoArray`](#TypeInfoArray)
  - [`TypeInfoIndirect`](#TypeInfoIndirect)
  - [`TypeInfoFunction`](#TypeInfoFunction)
  - [`TypeInfoFunctionRecord`](#TypeInfoRecord)
  - [`TypeInfoFunctionEnum`](#TypeInfoEnum)

<a name="TypeInfo"></a>
### `TypeInfo`, the base class
The base class for all other `TypeInfo` structs.
This is also used for primitive types like integers and floats.
```cpp
struct TypeInfo {
    TypeInfoType type;     // What derived TypeInfo class it uses.
    const char* type_name; // A stringified version of the type e.g. "POD"
    uint64_t type_id;      // A hash of the qualified type name (e.g. hash of "my_namespace::detail::POD")
    uint32_t size;         // The size in bytes (if applicable)
};
```

<a name="TypeInfoArray"></a>
### `TypeInfoArray`, for fixed-length arrays
```cpp
struct TypeInfoArray : public TypeInfo {
    const TypeInfo* underlying; // The underlying type
    int length;                 // The length of the array
};
```
If the array is multi-dimensional, the underlying type will be one dimension lower, e.g. `int[3][3][3]` -> `int[3][3]`.

<a name="TypeInfoIndirect"></a>
### `TypeInfoIndirect`, for references and pointers
```cpp
struct TypeInfoIndirect : public TypeInfo {
    const TypeInfo* underlying; // The underlying type
    IndirectType indirect_type; // What kind of indirect type it is (Pointer, L/R Reference)
};
```
Like with arrays, if the type is "layered", it will point to one layer lower, e.g. `void ***` -> `void **`.

<a name="TypeInfoFunction"></a>
### `TypeInfoFunction`, for functions and methods
```cpp
struct TypeInfoFunction : public TypeInfo {
    const char* name;              // The name of the function
    const TypeInfo* return_type;   // The return type
    int parameter_count;           // The parameter count
    FunctionParameter* parameters; // All parameters
    RecordAccess access;           // The access specifier (methods only, Public, Protected, Private)
    MethodType method_type;        // The method type (Normal, Virtual, Abstract)
    int tag_count;                 // The tag count
    const char** tags;             // All tags
};
```
While generating type data for functions and methods work, there are a few limitations:
  * With free standing functions, you can't index multiple functions with the same signature.
  
  `void some_func(int i, float f)` and `void other_func(int other_i, float some_f)` both have the same signature so they will collide.
  * Template functions can not be generated unless it's template is resolved by class/struct.
  
  This will work:
  ```cpp
template<typename T>
class Foo {
    T get_t();
};
```
  This will not:
```cpp
class Foo {
    template<typename T>
    T get_t();
}

// OR

template<typename T>
T get_t();
```

Additionally, each parameter looks like this:
```cpp
struct FunctionParameter {
    const TypeInfo* type_info; // The type
    const char* name;          // The name
};
```

<a name="TypeInfoRecord"></a>
### `TypeInfoRecord`, for structs, classes and unions
```cpp
struct TypeInfoRecord : public TypeInfo {
    RecordType record_type;     // What kind of record it is (Struct, Class, Union)
    int parent_count;           // How many parents it has
    const TypeInfo** parents;   // All parents
    int field_count;            // The field count
    RecordField* fields;        // All fields
    int method_count;           // The method count
    TypeInfoFunction* methods;  // All methods
    int tag_count;              // The tag count
    const char** tags;          // All tags
};
```
Additionally, each field looks like this:
```cpp
struct RecordField {
    const TypeInfo* type_info; // The type
    const char* name;    // The name
    uint32_t offset;     // The offset in bytes
    RecordAccess access; // The access specifier (Public, Private, Protected)
    int tag_count;       // The tag count
    const char** tags;   // All tags
};
```

<a name="TypeInfoEnum"></a>
### `TypeInfoEnum`, for enums
```cpp
struct TypeInfoEnum : public TypeInfo {
    const TypeInfo* underlying; // The underlying enum type
    int enum_count;             // The enum count
    const char** enum_names;    // All enumerators as strings
    int64_t* enum_values;       // All enumerator as integers
    int tag_count;              // The tag count
    const char** tags;          // All tags
};
```
