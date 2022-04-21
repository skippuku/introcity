# Attributes
Attributes define extra information about a member in a structure. They are defined with the `I` macro. The `I` macro is defined in `lib/types.h` as the following:
```C
#ifndef __INTRO__
#define I(...)
#endif
```

`__INTRO__` should defined by the *intro* preprocessor, but not by your compiler so that the *intro* parser can still see this information, but your compiler will expand it to nothing.    
The `I` macro must come after the member name but before the following `;` or `,`. Multiple attributes can be defined using commas inside the macro.    

```C
struct {
    int a I(id 1, =2);        // OK
    int b I(note "hello"), c; // OK
    char * name; I(id 3)      // ERROR
    I(4) bool is_open;        // ERROR
};
```

## Intro Attributes
These are the attributes provided by *intro*.

### id
**type:** [int](#int)   
**NOTE**: Serialization using IDs is planned but not currently implemented.    
Defines the id used for serialization. Every id in a structure must be unique. This is enforced by the parser.    
If there is a stray integer with no attribute defined, it is assumed to be an id, for convenience.
```C
    int   a I(id 1); // OK
    char  b I(2);    // OK
    float c I(1);    // ERROR, duplicate ID
```

### default
**type:** [value](#value)   
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

### note
**type:** [string](#string)    
Defines a note that will be displayed by [intro\_imgui\_edit](doc/LIB.md#intro_imgui_edit) when the mouse is hovered over the member.

## Custom Attributes
Attributes can be extended with custom attributes. These are defined by creating an `enum` with `I(attribute)` placed before the opening brace. New attributes are defined in this enumeration by placing an `I` macro after the name of the value which includes the attribute type followed by the name of the attribute being defined. The attribute type can be omitted for flag attributes.

```C
typedef enum I(attribute) {
    MY_ATTR_IGNORED I(flag ignored),
    MY_ATTR_SPECIAL I(special),       // same as "flag special"
    MY_ATTR_FRIEND  I(member friend),
    MY_ATTR_REFAULT I(value refault),
} MyAttribute;
```

The value of the attribute's id is defined by the enum value it is associated with. The range -16 to -1 (inclusive) is reserved by *intro*. Attributes must have unique IDs. Custom attributes can be used at any point after the enum is defined. In addition multiple enums can be used to define custom attributes as long as the IDs do not overlap.

## Attribute Types

### int
Attribute is defined as an integer of type `int32_t`.

### float
Attribute is defined as a number of type `float`.

### string
Attribute is defined like a string.

### member
Attribute is defined as a sibling member.

### value
Attribute is defined as a value of the type of the member it is applied to.

### flag
Attribute is defined with no data.


[intro_set_defaults]: ./LIB.md#intro_set_defaults
[intro_load_city]:    ./LIB.md#intro_load_city
[intro_create_city]:  ./LIB.md#intro_create_city
