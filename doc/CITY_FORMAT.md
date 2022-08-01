# CITY FILE FORMAT (.cty) version 0.4

A city file has three sections:
 - [Header](#header)
 - [Type Info](#type-info)
 - [Data](#data)

**City currently only supports Little-Endian!**

## Header

| Offset | Type  | Content |
|--------|-------|---------|
|0       |char[4]|[Magic number](#magic-number)|
|4       |u16    |[Version Major](#version)    |
|6       |u16    |[Version Minor](#version)    |
|8       |u8     |[Size Info](#size-info)      |
|9       |u8[3]  |Reserved/Unused              |
|12      |u32    |[Data Offset](#data-offset)  |
|16      |u32    |[Type Count](#type-count)    |
|20      |---    |[Type Info](#type-info)      |
|Data Offset|--- |[Data](#data)                |

### Magic Number
This is always ASCII `ICTY` (`0x49 0x42 0x54 0x59`)

### Version
For version 0.4, **Version Major** is 0 and **Version Minor** is 4.   
As this system is in infancy, and the format may undergo significant changes, only matching implementation and file versions are supported.   

### Size Info
This is a single byte containing the sizes used in the type info section.   
 - `TYPE_SIZE` is the 4 most significant bits plus 1.   
 - `PTR_SIZE` is the 4 least significant bits plus 1.   
An implementation extracting this information might look like this:   
```C
uint8_t size_info = header->size_info;
int TYPE_SIZE = 1 + ((size_info >> 4) & 0x0f);
int PTR_SIZE  = 1 + (size_info & 0x0f);
```
### Data Offset
This number is the offset from the begining of the file to the **DATA** section.

### Type Count
This is the number of types in the **TYPE INFO** section.


## Type Info

This section begins directly after the header. It contains a list of types. Types are laid out sequentially with no padding.   

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
   | Type                    | Content          |
   |-------------------------|------------------|
   |[`TYPE_SIZE`](#size-info)|reference type id |

   A pointer type may reference a type with an id greater than its own. It is the only category that may do this.

 - `INTRO_ARRAY`
   | Type                    | Content         |
   |-------------------------|-----------------|
   |[`TYPE_SIZE`](#size-info)|reference type id|
   |[`PTR_SIZE`](#size-info) |length           |

 - `INTRO_STRUCT` or `INTRO_UNION`
   | Type                   | Content    |
   |------------------------|------------|
   |[`PTR_SIZE`](#size-info)|member count|

   Following the member count is a list of members laid out like this:   
   | Type                      | Content |
   |---------------------------|---------|
   |[`TYPE_SIZE`](#size-info)  | type id |
   |[`PTR_SIZE`](#size-info)   | If the most significant bit is set, the rest of the bits define the member's id. Otherwise this is an offset into **DATA** where the member's name is located. |


## Data
At offset 0 in the data section is the data that was passed to `intro_create_city` to create the file. The data is transformed in the following ways:
 - Structs and arrays are serialiezd packed, with *no* alignment.
 - Unions have 2 header bytes which specify which member was serialized as a u16. Their size is equal to their largest member + the 2 header bytes.
 - Pointers are serialized as offsets into the Data section with size `PTR_SIZE`.
 - Scalars are unchanged.

The Data section also contains serialized data for pointers and member names.
