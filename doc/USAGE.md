# Usage

## intro.cfg
*intro* needs information about the compiler you will use, namely default preprocessor definitions and system include paths. If use clang of gcc, this can be generated with `intro --gen-config --compiler <gcc/clang> --file <output>`.   
*intro* looks for the configuration file in the following order and uses the first path that resolves to an existing file:
 - (CWD)/intro.cfg
 - (CWD)/.intro.cfg
 - ~/.config/introcity/intro.cfg (linux/msys)
 - %LOCALAPPDATA%/introcity/intro.cfg (windows)
 - (program directory)/intro.cfg
 - /etc/introcity/intro.cfg (linux/msys)

A configuration can also be manually passed using `--cfg <file>`.  
  
On linux, `sudo make install` will attempt to generate a config using the default cc compiler (usually gcc) and place it in /etc/introcity/intro.cfg.

## Running the Parser
*intro* creates a c header file based on a file you pass to it. It will create type information for every global type (*intro* does not look inside functions). To use this file, it must be included after the type definitions. *intro* ignores includes that end with "\*.intro" so it doesn't try to include the file it's about to generate.  

*intro* uses gcc-like preprocessor options such as `-D, -U, -I`. The output file can be specified with `-o`, otherwise it defaults to the input file with the ".intro" suffix appended.

## Parser Output
The `__intro` namespace is used to avoid any naming conflicts. At the end of the generated file `__intro_ctx` is defined which is used implicitly by most procudures in the library. Also important are the `ITYPE_` and `IATTR_` enum definitions.  
**NOTE:** The generated header makes use of flexible array memeber initialization. This is not supported by clang in c++ mode. I will probably be changing this in the future.

## Implicit Context
Many functions in the library require extra information provided by `__intro_ctx`. It would be cumbersome to manually pass this variable every time, so functions that use the context have associated macros that automatically do this for you. For example `intro_print` is actually a macro:
```C
#define intro_print(data, type, opt) intro_print_ctx(INTRO_CTX, data, type, opt)
```
`INTRO_CTX` is another macro that simply expands to `(&__intro_ctx)`.

## Type Information
`ITYPE` is the recommended way to get information about a type. It is defined with this line in `lib/types.h`.
```C
#define ITYPE(x) (&INTRO_CTX->types[ITYPE_##x])
```
This means that `ITYPE(Object)` will expand to `(&(&__intro_ctx)->types[ITYPE_Object])`.    
The `ITYPE_` enum values correspond to the index of the type with that name in the `__intro_types` array. This index is the type's ID.   
Note that types with names made of multiple identifiers, for example `struct Object` or `unsigned short` will have underscores replacing the spaces (use `ITYPE(struct_Object)` and `ITYPE(unsigned_short)`).   
Types with no name such as pointers and arrays do not have definitions generated in the enum currently.   
  
**See:** [Using Type Information](#using-type-information)

## Using Type Information
`ITYPE` expands to a pointer to a `IntroType` structure. The layout of this structure is as can be found in ([lib/intro.h](../lib/intro.h)).  
Some notes about types:  
 - the `of` member is used by pointers as well as arrays. It is also the return type of a function.
 - Not all types have a name. These include pointers, arrays, functions, and anonymous structs or enums.
 - Unions are treated like structs where all the members are at offset 0. This means `i_struct` is used to access union information.
 - c ints such as `int`, `unsigned short`, `char`, `long long unsigned int`, etc. are treated like typedefs of exact width integers `int32_t`, `uint16_6`, `int8_t`, and `uint64_t` respectively. This could be seen as the reverse of actuality, but because *intro/city* is focussed on serialization, this makes sense.
 - `bool` is treated as a type. *intro* disables macro expansion of `bool` to `_Bool` when it parses. If `_Bool` is used directly, it is treated like a typedef of `bool`. This was done for the sake of simplicity since `bool` is a type in c++.
 - Generated information isn't write protected, but you should not ever write to it.
