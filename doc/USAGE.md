# Reference
I recommend looking at the examples in the `test/` directory to better understand how *intro/city* can be used.    

## Parser Output
If you've built a test program there should exist a file at `test/test.h.intro`. This is what the output of the parser generally looks like. The header makes use of C99 struct initialization, so your compiler must support that. The `__intro` namespace is used to avoid any naming conflicts. The parts of this file you need to be aware of are `__intro_types`, `__intro_ctx`, and the big enum definition. These are used implicitly by the library with macros.

## Type Information
`ITYPE` is the recommended way to get information about a type. It is defined with this line in `lib/types.h`.
```C
#define ITYPE(x) (&__intro_types[ITYPE_##x])
```
This means that `ITYPE(Object)` will expand to `(&__intro_types[ITYPE_Object])`.    
The `ITYPE_` enum values correspond to the index of the type with that name in the `__intro_types` array. This index may be referred to as the type's ID.   
Note that types with names made of multiple identifiers, for example `struct Object` or `unsigned short` will have underscores replacing the spaces (use `ITYPE(struct_Object)` and `ITYPE(unsigned_short)`).   
Types with no name such as pointers and arrays do not have definitions generated in the enum currently.   

## Implicit Context
Many functions in the library require extra information provided by `__intro_ctx`. It would be cumbersome to manually pass this variable every time, so functions that use the context have associated macros that automatically do this for you. For example `intro_print` is actually a macro:
```C
#define intro_print(data, type, opt) intro_print_ctx(INTRO_CTX, data, type, opt)
```
`INTRO_CTX` is another macro that simply expands to `&__intro_ctx`.
