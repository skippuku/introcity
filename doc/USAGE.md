# Usage

## intro.cfg
*intro* needs information about the compiler you will use, namely default preprocessor definitions and system include paths. If use clang of gcc, this can be generated with `intro --gen-config --compiler <gcc/clang> --file <output>`.   
*intro* looks for the configuration file in the following order and uses the first path that resolves to an existing file:
 - (CWD)/intro.cfg
 - (CWD)/.intro.cfg
 - ~/.config/introcity/intro.cfg (linux/bsd)
 - %LOCALAPPDATA%/introcity/intro.cfg (windows)
 - (program directory)/intro.cfg
 - /etc/introcity/intro.cfg (linux/bsd)

A configuration can also be manually passed using `--cfg <file>`.  
  
On linux, `sudo make install` will attempt to generate a config using the default cc compiler (usually gcc) and place it in /etc/introcity/intro.cfg.

## Running the Parser
*intro* creates a c header file based on a file you pass to it. It will create type information for every global type (*intro* does not look inside functions). To use this file, it must be included after the type definitions. *intro* ignores includes that end with "\*.intro" so it doesn't try to include the file it's about to generate.  

*intro* uses gcc-like preprocessor options such as `-D, -U, -I`. The output file can be specified with `-o`, otherwise it defaults to the input file with the ".intro" suffix appended.

## Parser Output
The generated header makes use of flexible array member initialization, clang in c++ mode doesn't seem to support that. The `__intro` namespace is used to avoid any naming conflicts. The parts of this file you need to be aware of are `__intro_types`, `__intro_ctx`, and the `ITYPE_` enum definitions. These are used implicitly by the library with macros.

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
`ITYPE` expands to a pointer to a `IntroType` structure. The layout of this structure is as can be found in ([lib/intro.h](../lib/intro.h)).  
Some notes about types:  
 - Not all types have a name. These include pointers, arrays, and anonymous structs or enums.
 - Unions are treated like structs where all the members are at offset 0. This means `i_struct` is used to access union information.
 - The meaning of `parent` changes with context. For arrays and pointers, it is the type that the array is of or the pointer is to. For functions, it is the type of the return value. For any other category, it means the type is a typedef of the `parent` type.
 - c ints such as `int`, `unsigned short`, `char`, `long long unsigned int`, etc. are treated like typedefs of exact width integers `int32_t`, `uint16_6`, `int8_t`, and `uint64_t` respectively. This is the reverse of actuality.
 - `bool` is treated as a type. *intro* disables macro expansion of `bool` to `_Bool` when it parses. If `_Bool` is used directly, it is treated like a typedef of `bool`. This was done for the sake of simplicity since `bool` is a type in c++.
 - Generated information isn't write protected, but you should not ever write to it.
