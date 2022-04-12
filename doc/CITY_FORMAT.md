# CITY FILE FORMAT (.cty) version 0.1

A city file has three sections:
 - [Header](#header)
 - [Type Info](#type-info)
 - [Data](#data)

**City currently only supports Little-Endian!**

## Header
---------

| Offset | Type  | Content |
|--------|------ |---------|
|0       |char[4]|[Magic number](#magic-number)|
|4       |u16    |[Version Major](#version)    |
|6       |u16    |[Version Minor](#version)    |
|8       |u8     |[Size Info](#size-info)      |
|9       |u8[3]  |Reserved/Unused              |
|12      |u32    |[Data Offset](#data-offset)  |
|16      |u32    |[Type Count](#type-count)    |

### Magic Number
This is always ASCII `ICTY` (`49` `42` `54` `59` in hexadecimal)

### Version
For version 0.1, **Version Major** is 0 and **Version Minor** is 1.   
As this system is in infancy, and the format may undergo significant changes, only matching implementation and file versions are supported.   

### Size Info
This is a single byte containing the sizes used in the type info section.   
 - `TYPE_SIZE` is the last 4 (most significant) bits plus 1.   
 - `OFFSET_SIZE` is the first 4 (least significant) bits plus 1.   
An implementation extracting this information might look like this:   
```C
uint8_t size_info = header->size_info;
int TYPE_SIZE   = 1 + ((size_info >> 4) & 0x0f);
int OFFSET_SIZE = 1 + (size_info & 0x0f);
```
### Data Offset
This number is the offset from the begining of the file to the **DATA** section.

### Type Count
This is the number of types in the **TYPE INFO** section.


## Type Info
------------
This section begins directly after the header. It contains a list of types. The specification for a type is below. Types are laid out one after another with no padding.   

The order in which the types appear in the list specifies their "type id". The first type has type id 0 and the last has type id [Type Count](#type-count) - 1.

The last type in this list is the type of the struct that has been serialized.    
    
| Type | Content |
|------|---------|
|u8    |Category |

The first byte in a type is the category. This correlates to `IntroCategory`. Depending on the category there may be more information following this. Information is laid out without padding.   

 - Basic type (`INTRO_U8` <= category <= `INTRO_F64`)    
   No information follows a basic type. The next byte specifies a new category for a new type.

 - `INTRO_ENUM`
   | Type | Content |
   |------|---------|
   |u8    |Size     |

 - `INTRO_POINTER`
   | Type                    | Content      |
   |-------------------------|--------------|
   |[`TYPE_SIZE`](#size-info)|parent type id|

 - `INTRO_ARRAY`
   | Type                    | Content      |
   |-------------------------|--------------|
   |[`TYPE_SIZE`](#size-info)|parent type id|
   |u32                      | length       |

 - `INTRO_STRUCT` or `INTRO_UNION`
   | Type | Content    |
   |------|------------|
   |u32   |member count|

   Following the member count is a list of members laid out like this:   
   | Type                      | Content |
   |---------------------------|---------|
   |[`TYPE_SIZE`](#size-info)  | type id |
   |[`OFFSET_SIZE`](#size-info)| offset  |
   |[`OFFSET_SIZE`](#size-info)| offset into **DATA** where the member's name is located |


## Data
-------

At offset 0 in the data section is the struct that has been serialized. It is simply an exact copy expect that all pointers have either been zeroed or converted to offsets into the **DATA** section. Zeroed pointers are invalid.    

Following the serialized struct is miscellaneous data such as serliazed arrays or struct member names.
