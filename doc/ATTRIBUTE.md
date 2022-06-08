# Attributes
Attributes are used to define extra information about types they are declared and applied using the `I` macro. This macro expands to nothing unless `__INTRO__` is defined, so it does not affect compilation:
```C
#ifndef __INTRO__
#define I(...)
#endif
```

## Declaring Attributes
Attributes are created with the following syntax:
```C
I(attribute namespace_ (
    name1: type,
    name2: type,
    ...
))
```

Builtin attributes are defined in the same way as custom attributes. You can see how they are defined in [lib/intro.h](lib/intro.h).   
  
See the [types](#types) section for information on available attribute types.  

## Applying attributes
Attributes can be applied to struct member declarations and typedef declarations using the `I` macro.  
  
The `I` macro must directly preceed `;` or `,`. Multiple attributes can be defined using commas inside the macro.   
  
```C
struct {
    int a I(id 1, =2);        // OK
    int b I(note "hello"), c; // OK
    char * name; I(id 3)      // ERROR
    I(4) bool is_open;        // ERROR
};

typedef float Vector2 [2] I(vector, color {0,255,255,255});
```

Attributes can be referenced without their namespace if there are no name conflicts.  
  
Attributes can also be applied to a previously defined type using `apply_to`. For example the following is used in [lib/intro.h](lib/intro.h):
```C
I(apply_to (char *) (cstring))
```
The type must be contained in parenthesis like a cast. Following the type is a list of attributes as if they were declared directly on a member.

## Retrieving attributes

For information on how to retrieve attribute information in your program, see [LIB.md](doc/LIB.md).

# Builtin Attributes
These are the attributes provided by *intro*.

## Namespace: i_

### remove
Removes a previously applied attribute. Typically, the shorthand `~` is used.
```C
    int   some_constant I(remove gui_edit);
    float hidden I(~gui_show);
```

### id
**type:** [int](#int)   
Defines the id used for serialization. Every id in a structure must be unique and representable by a 16-bit unsigned integer. This is enforced by the parser.    
If there is a stray integer with no attribute defined, it is assumed to be an id, for convenience.
```C
    int   a I(id 1); // OK
    char  b I(2);    // OK
    char  b I(1985); // OK
    float c I(2);    // ERROR, duplicate ID
```

### btfld
**type:** [int](#int)
Can be used to retrieve the bitfield width of a member. It cannot be applied using the `I` macro.
```C
int32_t bit_width;
intro_attribute_int(member, i_btfld, &bit_width);
```

### default
**type:** [value(@inherit)](#value)   
**NOTE**: Some types might not support defaults. This is a work in progress.    
Defines the default used by [intro\_set\_defaults][intro_set_defaults] or [intro\_load\_city][intro_load_city] when no value is present in the city file.    
You can also use an equal sign `=` instead of the word default, for convenience.

```C
    int   a I(default 5);
    float b I(= 3.56);
    uint8_t arr [4] I(= {1, 2, 3, 4});
```

### length
**type:** [member](#member)   
Defines a sibling member which will be used to determine the length of a buffer during serialization in [intro\_create\_city][intro_create_city].
```C
    Tile * tiles I(length count_tiles);
    int count_tiles;
```

### alias
**type:** [string](#string)   
Defines a name which will be treated like a match by [intro\_load\_city][intro_load_city].    
Use this if you are using member names for serialization and you change a name.    
While this attribute is of type *string*, quotation marks `"` can be omitted for convenience.
```C
    int health_points I(alias "hp");
    float speed I(alias velocity);
```

### city @global
**type:** [flag](#flag)
Determines that a value that is a pointer will be serialized by the city implementation.
```C
    CacheMap * internal_cache I(~city);
```

### cstring
**type:** [flag](#flag)
Determines that a pointer is to a null-terminated string. This is automatically applied to any value of type `char *`.
```C
struct MyString {
    char * str I(length len, ~cstring);
    int len;
};
```

### type
**type:** [flag](#flag)   
Marks the member to be set to the type of the structure it is within during `intro_set_defaults` or `intro_load_city`.    
Can only be applied to a member of type `IntroType *`.
```C
typedef struct {
    int a, b, c;
    IntroType * type I(type); // will be set to ITYPE(Entity)
} Entity;
```

## Namespace: gui_

Attributes in the `gui_` namespace are used by [intro\_imgui\_edit](lib/intro_imgui.cpp).

### note
**type:** [string](#string)   
Defines a "help" note to be displayed with a member.

### name
**type:** [string](#string)
Defines an alternative "friendly name" for a member.

### min
**type:** [value(@inherit)](#value)
Defines a minimum allowed value for a scalar.

### max
**type:** [value(@inherit)](#value)
Defines a maximum allowed value for a scalar.

### format
**type:** [string](#string)
Defines the display format used for a scalar.
```C
    float speed I(gui_format "%3.2f");
```

### color
**type:** [value(uint8\_t [4])](#value)
Changes the color of a member.

### vector
**type:** [flag](#flag)
Determines that a member is a vector.

### show @global
**type:** [flag](#flag)
Determines that a member is displayed.

### edit @global
**type:** [flag](#flag)
Determines that a member can be edited by the user.

### edit\_color
**type:** [flag](#flag)
Determines that a member is a color, and uses a specialized color editing widget.

### edit\_text
**type:** [flag](#flag)
Determines that a member is a buffer of editable text.

# Attribute Types

### int
Attribute is defined as an integer of type `int32_t`.

### float
Attribute is defined as a number of type `float`.

### string
Attribute is defined as a null-terminated string.

### member
Attribute is defined as a sibling member.

### value
Attribute is defined as a value of a specific type. The type is determined with parentheses:
```C
    some_value: value(MyType),
```

The type can also be defined as the type of the member or type it is applied to using @inherit.
```C
    some_default: value(@inherit),
```

### flag
Attribute is defined with no data. Flags can have the `@global` trait which means that they are applied to every type and member by default.


[intro_set_defaults]: ./LIB.md#intro_set_defaults
[intro_load_city]:    ./LIB.md#intro_load_city
[intro_create_city]:  ./LIB.md#intro_create_city
