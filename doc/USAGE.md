# Usage

## Running the Parser
The parser creates a c header file based on a file you pass to it. It will create type information for every global type (*intro* does not look inside functions). To use this file, it must be included after the type definitions. There are a couple of ways to handle this.

### separate header
If a header includes all of types that need information generated, run the *intro* on the header, then include the generated file after the header in the .c file. This method is employed by [test.c](../test/test.c) and [city\_test.c](../test/city_test.c).   
**Example:**
```C
#include "intro/lib/intro.h"
#include "my_types.h"
#include "my_types.h.intro"
```

### conditional exclusion
If it is more convenient to run the parser on code that uses *intro* information, the generated header include will need to be excluded so that when the parser runs on the file, it does not try to include the file it hasn't generated yet. This can be done by checking for the `__INTRO__` macro which is defined by the parser's preprocessor. This method is employed by [interactive\_test.c](../test/interactive_test.c)    
**Example:**
```C
#include "intro/lib/intro.h"

typedef MyStruct {
    int a, b, c;
    char * name;
};

#ifndef __INTRO__
# include "main.c.intro"
#endif
```

## Parser Output
The generated header makes use of C99 struct initialization, so your compiler must support that. The `__intro` namespace is used to avoid any naming conflicts. The parts of this file you need to be aware of are `__intro_types`, `__intro_ctx`, and the `ITYPE_` enum definitions. These are used implicitly by the library with macros.

## Type Information
`ITYPE` is the recommended way to get information about a type. It is defined with this line in `lib/types.h`.
```C
#define ITYPE(x) (&__intro_types[ITYPE_##x])
```
This means that `ITYPE(Object)` will expand to `(&__intro_types[ITYPE_Object])`.    
The `ITYPE_` enum values correspond to the index of the type with that name in the `__intro_types` array. This index may be referred to as the type's ID.   
Note that types with names made of multiple identifiers, for example `struct Object` or `unsigned short` will have underscores replacing the spaces (use `ITYPE(struct_Object)` and `ITYPE(unsigned_short)`).   
Types with no name such as pointers and arrays do not have definitions generated in the enum currently.   
  
**See:** [Using Type Information](#using-type-information)

## Implicit Context
Many functions in the library require extra information provided by `__intro_ctx`. It would be cumbersome to manually pass this variable every time, so functions that use the context have associated macros that automatically do this for you. For example `intro_print` is actually a macro:
```C
#define intro_print(data, type, opt) intro_print_ctx(INTRO_CTX, data, type, opt)
```
`INTRO_CTX` is another macro that simply expands to `&__intro_ctx`.

## Using Type Information
`ITYPE` expands to a pointer to a `IntroType` structure. The layout of this structure is as follows: ([lib/intro.h](../lib/intro.h))
```C
struct IntroType {
    char * name;
    IntroType * parent;
    IntroCategory category;
    union {
        uint32_t array_size;
        IntroStruct * i_struct;
        IntroEnum * i_enum;
    };
};
```
Some notes about types:  
 - Not all types have a name. These include pointers, arrays, and anonymous structs or enums.
 - Unions are treated like structs where all the members are at offset 0. This means `i_struct` is used to access union information.
 - The meaning of `parent` changes with context. For arrays and pointers, it is the type that the array is of or the pointer is to. For any other category, it means the type is a typedef of the `parent` type.
