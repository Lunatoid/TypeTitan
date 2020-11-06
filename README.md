# TypeTitan
![Build](https://github.com/Lunatoid/TypeTitan/workflows/Build/badge.svg)

A type data generator for C++ without RTTI, macros and with minimal templates.

# Table of Contents
 - [Introduction](#intro)
 - [Download](#download)
   - [How to build](#build-howto)
 - [How to use](#usage-howto)
   - [Command-line options](#cmd-options)
   - [Generated files](#gen-files)
   - [How to index a type](#index-howto)
     - [Additional indexing options](#opt-index)
   - [Including type data](#include-howto)
   - [Getting type data](#types-howto)
   - [Using type data](#using-types-howto)
   - [Calling methods](#calling-methods)
   - [Utility functions](#util-funcs)
 - [How it works](#how-it-works)
 - [Limitations](#limitations)
   - [Invoking free functions](#limit-func-invoke)
   - [Template indexing](#limit-template-index)
   - [Multiple-inheritance offsets](#limit-multi-offset)
 - [License](#license)

<a name="intro"></a>
# Introduction
TypeTitan is a type info generator that uses libClang and C++17.
The generated files are compatible with C++11 and later.

TypeTitan can generate type info for:
  - primitives (int, float, etc)
  - fixed-length arrays
  - indirect types (pointers, references)
  - functions/methods
  - records (structs, classes, unions)
  - enums

<a name="download"></a>
# Download
You can download the latest version from the [Actions](https://github.com/Lunatoid/TypeTitan/actions) page.

The latest version should be at the top and then you can download it under the "Artifacts" section.

<a name="build-howto"></a>
## How to build
External dependencies for building:
  - [Clang 10.0](https://releases.llvm.org/download.html)

On Windows, you can download `windows-x64-libclang` to
get `libclang.dll` if you don't want to download LLVM.

<a name="usage-howto"></a>
# How to use
The most basic usage is as follows:
```
$ tt path/to/src path/to/src/*.h
```
This will generate the basic `type_titan.h`/`type_titan.inc.h`/`type_titan.util.h` files in `/path/to/src`.
This will also generate a `.tt.h` file for every header file in `path/to/src`.

<a name="cmd-options"></a>
## Command-line options
The format is as follows:
```
$ tt <output> [options...] <files...>
```

You can specify the following options

| option          | description                                                      |
|-----------------|------------------------------------------------------------------|
| `-help`         | displays the help screen                                         |
| `-namespace`    | sets the namespace name                                          |
| `-no-extas`     | disables the generation of the `inc`, `init` and `util` files    |
| `-no-empty`     | deletes all generated files that do not have any type info       |
| `-no-recursive` | disables recursively searching the provided directories          |
| `-clang`        | passes all subsequent arguments to the clang parser              |

One thing to note is that arguments are parsed as they come in, so it's possible to do things like this:
```
$ tt recursively/search/this/*.h -no-recursive other/folder/*.h
```

<a name="gen-files"></a>
## Generated files
TypeTitan generates a few core files:
  - `type_titan.h`
    - Contains all the core definitios, also contains type info for all primitive-based type (like `int` or `void*`)
  - `type_titan.inc.h`
    - Includes every generated file
  - `type_titan.util.h`
    - Contains helper functions for ease-of-use

<a name="index-howto"></a>
## How to index a type
To tell TypeTitan what to generate you must add `//!!` in front of the type declaration.
```cpp
//!!
struct POD {
  int i;
  float x;
};

//!!
int some_func(float f = 5.0f) {
  // ...
}

//!!
template<typename T, int size>
class Array {
  T arr[size];
};

```
That is all that is necessary to generate type info for the desired declarations.

No macro or template voodoo hell!

<a name="opt-index"></a>
### Additional indexing options
If you don't want a specific field, method or enum value indexed you can add `DoNotIndex` to the comment.
You can also add tags to a type with `Tags=A,B,C`. Tags are seperated by a comma and cannot contain spaces
```cpp
//!! Tags=SomeTag,OtherTag
struct POD {
  int i;
  float f;
  
  //!! DoNotIndex
  void* ptr;
};

//!!
enum class Fruit {
  Apple,
  Pear,
  //!! DoNotIndex
  Lettuce
};
```

<a name="include-howto"></a>
## Including type data
For every file TypeTitan will generate a new file with the same name that ends in `.tt.h`.
This file contains all the type info and also includes the original file. Just include the newly generated file instead of the original one!

```cpp
#include "foo/bar.h"
// Becomes
#include "foo/bar.tt.h"
```

<a name="types-howto"></a>
## Getting type data
The namespace `tt` has been ommitted from these examples.
```cpp
POD pod;

const TypeInfo* pod_ti = type_of<POD>();
const TypeInfo* pod_ti = type_of(pod);
```

<a name="using-types-howto"></a>
## Using type data
#### Method 1: manually using type data
```cpp
const TypeInfo* ti = type_of<POD>();

if (ti->type == TypeInfoType::Record) {
  const TypeInfoRecord* tir = (const TypeInfoRecord*)ti;
  for (int i = 0; i < tir->field_count; i++) {
    printf("%s\n", tir->fields[i].name);
  }
}
```

#### Method 2: using the utility file:
If `-no-extas` is not specified TypeTitan will generate a `type_titan.util.h` files that has a bunch of helper functions.
```cpp
for (auto& field : get_fields<POD>()) {
  printf("%s\n", field.name);  
}
```

<a name="calling-methods"></a>
## Calling methods
You can use the `call_method` function with the first template parameter being the return type like so:
```cpp
Foo foo;
bool success = false;

int i = call_method<int>(foo, success, "add", 5, 10);

if (success) {
  // Function was successfully called
}
```
If you don't care about whether the call was successful or not you can omit the `success` parameter.
```cpp
Foo foo;
int i = call_method<int>(foo, "add", 5, 10);
```

### What functions can you call
```cpp
//!!
template<typename T>
class Foo {
public:
  void yay(); // OK
  T get_t(); // OK, T is known
  
  // Overloads are OK
  float add(float a, float b);
  int add(int a, int b);
  
  template<typename U>
  U get_u(); // Not OK, U is not known

private:
  void secret(); // Not OK, can't access non-public members
};
```

<a name="util-funcs"></a>
## Utility functions
In the `type_titan.util.h` file you can find a bunch of helper functions.
Most of these functions can be called in a variety of ways, for example `has_tag`:
```cpp
// Valid ways to call has_tag:
has_tag(type_info, "SomeTag"); // const TypeInfo* or any derived
has_tag(record_field, "SomeTag"); // RecordField*
has_tag<POD>("SomeTag"); // T
has_tag(pod, "SomeTag"); // T& parameter
```

Function documentation [here](https://github.com/Lunatoid/TypeTitan/tree/master/docs/utils.md).

<a name="how-it-works"></a>
# How it works
TypeTitan works by instantiating template classes with static functions.
This allows for partial template specialization, which means that templated types
are supported.

For calling methods there is another class, which gets instantiated
with the arguments and return type, then there is a function which
gets called if the cast of the member function pointer is successful
and a templated back-up function to avoid a compile error and signal
failure in invoking the desired function.

All the type info is statically allocated and only assigned once, upon
the first fetch, which should make it performant to get.

<a name="limitations"></a>
# Limitations
<a name="limit-func-invoke"></a>
## Invoking free functions
Invoking free functions based off their type info is not possible.

<a name="limit-template-index"></a>
## Indexing templates
Template fuctions are not indexed if their template type is not known.
```cpp
template<typename T>
class Foo {
  T get_t(); // OK
}

class Foo {
  template<typename T>
  T get_t(); // Can't generate
}

template<typename T>
T get_t(); // Can't generate
```

<a name="limit-multi-offset"></a>
## Multiple-inheritance offset
If you're using multiple inheritance it might be difficult to get the proper offsets for other base classes.

Single-inheritance doesn't have this problem since the base class will start from offset 0, but with multiple
base classes getting the offset is not that easy because the ABI doesn't enforce a standard layout.

<a name="license"></a>
# License
TypeTitan and code generated by it are licensed under the MIT license.
For more information, check [LICENSE.TXT](https://github.com/Lunatoid/TypeTitan/blob/master/LICENSE.txt)
