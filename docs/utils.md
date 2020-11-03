# Utility file
The utility file `type_titan.utils.h` is full of helper functions to avoid having
to manually deal with all the type info.

## Calling convention
Many of these functions have many overloads to make calling them as easy as possible.
Here is an example of all the ways to call `get_tags`:
```cpp
get_tags(const TypeInfo*); // TypeInfo* or any derived
get_tags(RecordField*)
get_tags<T>();
get_tags(my_var); // T is deduced here
```
Most functions below can be called in these ways, so the first parameter has been omitted.

## Functions
```cpp
Span<RecordField> get_fields()
```
Returns:
  - A span of all fields. If the type is not a record it will return an empty span.

---

```cpp
Span<const TypeInfo*> get_parents()
```
Returns:
  - A span of all parent classes. If the type is not a record it will return an empty span.

---

```cpp
Span<TypeInfoFunction> get_methods()
```
Returns:
  - A span of all methods. If the type is not a record it will return an empty span.

---

```cpp
Span<FunctionParameter> get_parameters()
```
Returns:
  - A span of all the function parameters. If the type is not a function it will return an empty span.

---

```cpp
Span<const char*> get_enum_names()
```
Returns:
  - A span of all stringified enum names. If the type is not an enum it will return an empty span.

---

```cpp
Span<int64_t> get_enum_values()
```
Returns:
  - A span of all the enum values. If the type is not an enum it will return an empty span.

---

```cpp
Span<const char*> get_tags()
```
Returns:
  - A span of all the tags. If the input doesn't have any tags it will return an empty span.

---

```cpp
RecordField* get_field(const char* field_name)
```
Parameters:
  - `field_name`:
    - The name of the field to find.

Returns:
  - A pointer to the field. If the field was not found it will return a `nullptr`.
  
---

```cpp
TypeInfoFunction* get_method(const char* method_name)
```
Parameters:
  - `method_name`:
    - The name of the method to find.

Returns:
  - A pointer to the method. If the method was not found it will return a `nullptr`.

---

```cpp
FunctionParameter* get_parameter(const char* parameter_name)
```
Parameters:
  - `parameter_name`:
    - The name of the parameter to find.

Returns:
  - A pointer to the parameter. If the parameter was not found it will return a `nullptr`.

---

```cpp
bool has_tag(const char* tag)
```
Parameters:
  - `tag`:
    - The tag to check.

Returns:
  - Whether or not it has the specified tag.

---

```cpp
const TypeInfo* get_underlying()
```
Returns:
  - Returns the underlying type. If there is no underlying type it will return a `nullptr`.

---

```cpp
RecordAccess get_access()
```
Returns:
  - Returns the access specifier. If there is no access specifier it will return `RecordAccess::Private`.

---

```cpp
template<typename T>
const T* cast(const TypeInfo*)
```
Info:
  - Casts a `const TypeInfo*` to a derived class, for example: `const TypeInfoEnum* enum_ti = cast<TypeInfoEnum>(ti)`.
  - Note that this isn't a generic `dynamic_cast` function, this is solely for casting `TypeInfo*`

Returns:
  - Returns the casted pointer if the types match. Otherwise it will return a `nullptr`.

---

```cpp
enum ReadWritePolicy {
    Type,
    ExactSize
    FittingSize
    Raw
};
```
Values:
  - `Type`:
    - The `data` must be the exact same type as the field.
  - `ExactSize`:
    - The `data` must be the exact same size as the field. Type can be different.
  - `FittingSize`:
    - The `data` must be the exact same size or smaller as the field. Type can be different.
  - `Raw`:
    - No checks are performed. Note that this will **not** write out of bounds.
 
 ---
 
```cpp
template<typename T, typename U>
bool write_field(const T& target, const char* field_name, const U& data, ReadWritePolicy policy)
bool write_field(const T& target, RecordField* field, const U& data, ReadWritePolicy policy)
```
Parameters:
  - `target`:
    - The target record to write to.
  - `field_name`/`field`:
    - Either the name of the field to write to or the `RecordField*` directly.
  - `data`:
    - The data to write to the desired field.
  - `policy`:
    - The type checking policy of the read/write operation.

Returns:
  - `true` if it successfully wrote, `false` if otherwise.

---

```cpp
template<typename T, typename U>
bool read_field(const T& target, const char* field_name, const U& data, ReadWritePolicy policy)
bool read_field(const T& target, RecordField* field, const U& data, ReadWritePolicy policy)
```
Parameters:
  - `target`:
    - The target record to read from.
  - `field_name`/`field`:
    - Either the name of the field to read from or the `RecordField*` directly.
  - `data`:
    - The variable to read the field into. The field value will be `memcpy`'d into the address of this variable.
  - `policy`:
    - The type checking policy of the read/write operation.

Returns:
  - `true` if it successfully read, `false` if otherwise.

---

### Note for `read_field`/`write_field`:
The amount of bytes to read/write is `min(field_size, data_size)`, so there are no out-of-bounds writes, even if `ReadWritePolicy::Raw` is specified.

