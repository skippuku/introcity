# intro/city

The *intro/city* project consists of tooling for effortless introspection and serialization of C data.
 - *intro* is parser which generates a c header with information about the types in your program.
 - *city* is a file format for arbitrary data serialization which is implemented in *introlib*.

## Features
 - **No Friction**    
    - *intro* parses your already existing C code. No need to learn a new way to define a structure type.
    - No accessors. When you load city data, it is written directly to your structure.
    - City does not need IDs on every struct member. It can use the members' name. If you change the name of a member, you can alias to the old name using an [attribute](doc/ATTRIBUTE.md#alias).
 - **Simple**    
    - *intro* does not depend on any external parsing library. It even uses its own preprocessor.
    - The library is only about 1000 lines. The city implementation is only a few hundred lines.
    - The generated header is made up of pure data, no functions. The data does not even need to be initialized in any way.
    - The project is written in plain C99 code. No dyslexia-inciting C++ jargon. This also means APIs can be written for other languages.
 - **Attributes**
    - Attributes provide extra information about a piece of data. They are defined with the `I` macro. They can be used for a variety of purposes, for example the `length` attribute defines a member which is the length of a buffer for serialization.
    - You can create **custom attributes** for your own purposes. This might include information for an interface such as ranges and precision, or alternative defaults for different situations, or anything else you can think of.
    - There are a variety of attribute types including *int*, *float*, *member*, *value*, and *expr*. For more information, see [doc/ATTRIBUTE.md](doc/ATTRIBUTE.md).

## Minimal Example

### [test/interactive\_test.c](test/interactive_test.c)
```C
#include <intro.h>

typedef int32_t s32;
typedef struct {
    s32 demo I(fallback 22);
    char * message I(fallback "empty message");
} SaveData;

#include "interactive_test.c.intro" // generated

int
main() {
    SaveData save;

    bool have_file = intro_load_city_file(&save, ITYPE(SaveData), "save.cty");
    if (!have_file) {
        intro_set_fallbacks(&save, ITYPE(SaveData));
    }

    printf("Save file contents:\n");
    intro_print(&save, ITYPE(SaveData), NULL);

    char new_message [1024];
    printf("Enter a new message: ");
    get_input_line(new_message, sizeof new_message);

    save.message = new_message;
    save.demo++;

    intro_create_city_file("save.cty", &save, ITYPE(SaveData));

    return 0;
}
```

### Output
```console
$ ./interactive_test
Save file contents:
{
    demo: s32 = 22;
    message: *char = "empty message";
}
Enter a new message: Hello there!
$ ./interactive_test
Save file contents:
{
    demo: s32 = 23;
    message: *char = "Hello there!";
}
```

For more examples, check out the test directory.

## Build/Install
`make release` will build a release version of the parser at build/release/intro.   
You may want to run `make test` to ensure everything is working correctly.  

### Linux / MSYS2
`sudo make install` should set you up so you can invoke "intro" from anywhere.

## Integration
To integrate *intro/city* in your code, you can use the *intro* parser to generate a C header from a source file then include the generated header somewhere. You will also want to link *introlib*.
*intro* can trivially be inserted as a step in the build process. For example a Makefile might look like this:

```Makefile
INTRO_LIB := path/to/intro/lib/

main: main.c data_types.h.intro introlib.o
    $(CC) -o $@ main.c introlib.o -I$(INTRO_LIB)
    
data_types.h.intro: data_types.h
    intro -o $@ data_types.h

introlib.o: $(INTRO_LIB)introlib.c $(INTRO_LIB)intro.h
    $(CC) -o $@ $<
```

`lib/intro.h` must be included *before* your types are declared.

## Disclaimers

 - *intro/city* is currently in beta. APIs and implementations are subject to change. If you intend to use this seriously, please be careful about updating. If you run into problems or friction that I am not aware of, please create a new issue on [github](https://github.com/cyman-ide/introcity).
 - *intro/city* currently only supports x86\_64. This may change in the future.
 - the *intro* parser is currently not aware of C++ concepts such as `private` or methods. Only C99 features are fully supported. This may change in the future. You can simply keep relevant types in a seperate file.
 - Security against foreign data is not a priority. Don't use these tools with data from a source you don't trust.

## Documentation
Documentation is provided in the [doc/](doc/) directory. It may not always be complete, but should cover the important things. It may be useful to look at the tests in [test/](test/) or the library source in [lib/](lib/).    
 - [usage (USAGE.md)](doc/USAGE.md)
 - [library reference (LIB.md)](doc/LIB.md)
 - [attribute reference (ATTRIBUTE.md)](doc/ATTRIBUTE.md)

## Credits
*intro/city* makes extensive use of `stb_ds.h` and `stb_sprintf.h` from the [stb libraries](https://github.com/nothings/stb) provided by Sean Barrett.
