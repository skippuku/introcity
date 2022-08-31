# Attributes
Attributes are used to define extra information about types they are declared and applied using the `I` macro. This macro expands to nothing unless `__INTRO__` is defined, so it does not affect compilation:
```C
#ifndef __INTRO__
#define I(...)
#endif
```

## Terminology
```
              Namespace
              v
  I(attribute my_ (
      category_string: value(char *) @propagate,   <-- Attribute Type Declaration
      ^                ^              ^
      Type             Category       Trait
  ))

    Attribute     Namespace Label
    v---------v   v
  I(fallback -1, gui: min -1, max 100) int some_member;
   |_________________________________|-> Attribute Directive
```

## Declaring Attribute Types
Attribute types are created with the following syntax:
```C
I(attribute namespace_ (
    name1: category,
    name2: category,
    ...
))
```

Builtin attribute types are defined in the same way as custom attribute types. You can see how they are defined in [lib/intro.h](../lib/intro.h).   
  
See the [categories](#attribute-categories) section for information on available attribute categories.  

## Applying Attributes
Attributes can be applied to struct member declarations, typedef declarations, and enum values using an attribute directive.  
  
The directive can be placed directly before a declaration or directly after, preceeding the `;` or `,`. Multiple attributes can be applied using multiple directives or with commas.
  
```C
struct {
    int a I(id 1, =2);          // OK

    int b I(gui_note "hello"),
        c I(~city) I(alias c0); // OK

    char * name; I(id 3)        // OOPS! attribute 'id 3' will be aplied to is_open, not name

    I(4) bool is_open;          // OK

    I(gui_note "many, many attributes")
    I(gui: min -1.0, max 12.0, scale 0.05)
    float stuff I(id 7);        // OK
};

typedef float Vector2 [2] I(vector, color {0,255,255,255});

enum SomeEnum {
    SOME_FIRST I(id 0),     // OK
    SOME_SECOND = 25 I(1),  // OK (must be after the assignment expression in this case)

    SOME_PENULTIMATE, I(2) // WOOPS! this would get applied to SOME_LAST

    I(3)
    SOME_LAST,  // OK
};
```
  
Attributes can also be applied to a previously defined type using `apply_to`. For example the following is used in [lib/intro.h](../lib/intro.h):
```C
I(apply_to (char *) (cstring))
```
The type must be contained in parenthesis like a cast. Following the type is an attribute directive.

## Namespacing rules

Attributes are declared with a namespace like `gui_`. That namespace gets prepended to all of the attribute type names. To avoid verbosity you can use a label to automatically prepend a namespace in an attribute directive. the global namespace can still be used, but if there exists an attribute type in the currently used namespace with the same name, the namespaced attribute takes priority.

```C
I(attribute My_ (
    special:  flag,
    joint:    member,
    alias:    float,
    fallback: value(@inherit),
))

I(My_special,
  // directives always start in the global namespcae. alias refers to the built-in attribute here
  alias "name",

  // if the namespace ends with an underscore, the underscore can be optionally ommitted in the label
  gui: note "im stuff", My_special,

  // fallback refers to My_fallback, but id still refers to the built-in id from the global namespace
  My: joint some_member, fallback {-1.0, 1.0}), id 5,

  // you can return to the global namespace with @global, now fallback refers to the built-in attribute type
  @global: fallback {0, 0},
)
typedef Vector2 MyVec2;
```

## Retrieving attributes

For information on how to retrieve attribute information in your program, see [LIB.md](LIB.md).

# Builtin Attributes
These are the attributes provided by *intro*.

## Namespace: @global

### remove
Removes a previously applied attribute. Typically, the shorthand `~` is used. Useful for removing global or propagated attributes.
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

### bitfield
**type:** [int](#int)
Can be used to retrieve the bitfield width of a member. It cannot be applied using the `I` macro.
```C
int32_t bit_width;
intro_attribute_int(member, bitfield, &bit_width);
```

### fallback @propagate
**type:** [value(@inherit)](#value)   
Defines the default used by [intro\_load\_city][intro_load_city] when no value is present in the city file.    
You can also use an equal sign `=` instead of the word default, for convenience.

```C
    int   a I(fallback 5);
    float b I(= 3.56);
    uint8_t arr [4] I(= {1, 2, 3, 4});
```

### length
**type:** [expr](#expr)   
Defines a sibling member which will be used to determine the length of a buffer during serialization in [intro\_create\_city][intro_create_city].
```C
    Tile * tiles I(length count_tiles);
    int count_tiles;
```

### alias
**type:** [value(char \*)](#value)   
Defines a name which will be treated like a match by [intro\_load\_city][intro_load_city].    
Use this if you are using member names for serialization and you change a name.    
While this attribute is of type *char \**, a single identifier without quotation marks is also accepted.
```C
    int health_points I(alias "hp");
    float speed I(alias velocity);
```

### imitate @propagate
**type:** [type](#type)
Defines a type that a variable will be interpreted as in specific situations. For example, it can be used to to display an integer as a specific enum type.

### header @propagate
**type:** [type](#type)
Defines a type that is used as a pointer header such as in certain dynamic array implementations. When this attribute is applied, expression attributes will read the header instead of the parent type.
```C
    int * dynamic_array I(header stb_array_header, length length); // length will be read from the array header
```

### city @global
**type:** [flag](#flag)
Determines that a value that is a pointer will be serialized by the city implementation.
```C
    CacheMap * internal_cache I(~city);
```

### cstring @propagate
**type:** [flag](#flag)
Determines that a pointer is to a null-terminated string. This is automatically applied to any value of type `char *`.
```C
struct MyString {
    char * str I(length len, ~cstring);
    int len;
};
```

## Namespace: gui_

Attributes in the `gui_` namespace are used by [intro\_imgui\_edit](../lib/intro_imgui.cpp).

### note
**type:** [value(char \*)](#value)   
Defines a "help" note to be displayed with a member.

### name
**type:** [value(char \*)](#value)
Defines an alternative "friendly name" for a member.

### min @propagate
**type:** [value(@inherit)](#value)
Defines a minimum allowed value for a scalar.

### max @propagate
**type:** [value(@inherit)](#value)
Defines a maximum allowed value for a scalar.

### format @propagate
**type:** [value(char \*)](#value)
Defines the display format used for a scalar.
```C
    float speed I(gui_format "%3.2f");
```

### color @propagate
**type:** [value(uint8\_t [4])](#value)
Changes the color of a member.

### vector @propagate
**type:** [flag](#flag)
Determines that a member is a vector.

### show @global
**type:** [flag](#flag)
Determines that a member is displayed.

### edit @global
**type:** [flag](#flag)
Determines that a member can be edited by the user.

### edit\_color @propagate
**type:** [flag](#flag)
Determines that a member is a color, and uses a specialized color editing widget.

### edit\_text @propagate
**type:** [flag](#flag)
Determines that a member is a buffer of editable text.

# Attribute Categories

### int
Attribute is defined as an integer of type `int32_t`.

### float
Attribute is defined as a number of type `float`.

### member
Attribute is defined as a sibling member.

### type
Attribute is defined as a C type.

### value
Attribute is defined as a value of a specific type. The type is determined with parentheses:
```C
    some_value: value(MyType),
```

The type can also be defined as the type of the member or type it is applied to using @inherit.
```C
    some_default: value(@inherit),
```

### expr
Attribute is defined as an expression that is converted to bytecode. Currently the expression can only use integer values.   
The following is an example using the builtin "when" attribute which is of the expr type.
```C
struct Variant {
    union {
        int i   I(when <-type == T_INT);   // the <- operator accesses the parent
        float f I(when <-type == T_FLOAT);
    } value;

    enum {
        T_NONE,
        T_INT,
        T_FLOAT,
    } type;
};
```

### flag
Attribute is defined with no data. Flags can have the `@global` trait which means that they are applied to every type and member by default.

# Traits

## @propagate
The `@propagate` trait can be applied to an attribute type of any category. Attributes of a propagated type will be inherited by typedefs and members of a C type.

## @transient
Transient attributes only exist during the parse pass. They cannot be checked for at runtime. They do not store information.

## @imply
Used to define an attribute directive that will be applied along with the attribute. Often combined with @transient as a way to conveniently apply several attributes.
```C
I(attribute my_ (
    favorite: flag @imply(gui_color {255,255,0,255}),
    dyn_array: flag @transient @imply(header MyArrayHeader, length .count),
    percent: flag @transient @imply(gui: min 0, max 0, format "%2.f"),
))
```

[intro_fallback]:     ./LIB.md#intro_fallback
[intro_load_city]:    ./LIB.md#intro_load_city
[intro_create_city]:  ./LIB.md#intro_create_city
