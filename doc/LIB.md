# intro library reference
**NOTE:** *intro/city* is in beta. API is subject to change.    

# CITY implementation

### `intro_create_city`
```C
void * intro_create_city(const void * src, const IntroType * src_type, size_t * o_size);
```
Create CITY data from `src`, write the size of the data to `o_size`, and return a pointer to the data.   
Returns `NULL` on failure.

**example:**
```C
Object obj = create_object();
size_t city_size;
void * city_data = intro_create_city(&obj, ITYPE(Object), &city_size);
dump_to_file("obj.cty", city_data, city_size);
```

### `intro_load_city`
```C
int intro_load_city(void * dest, const IntroType * dest_type, void * city_data, size_t city_data_size);
```
Load CITY data into `dest`. This may set pointers inside `dest` to point to data in `city_data`, be aware of this before freeing `city_data`.
Returns non-zero on failure.

**example:**
```C
Object obj;
size_t city_size;
void * city_data = read_entire_file("obj.cty", &city_size);
int error = intro_load_city(&obj, ITYPE(Object), city_data, city_size);
```

# type information

### `intro_is_scalar`
```C
INTRO_INLINE bool intro_is_scalar(const IntroType * type);
```
Return true if `type` is an integer or floating point number.

### `intro_is_int`
```C
INTRO_INLINE bool intro_is_int(const IntroType * type);
```
Return true if `type` is an integer.

### `intro_is_complex`
```C
INTRO_INLINE bool intro_is_complex(const IntroType * type);
```
Return true if `type` is a struct, union, or enum.

### `intro_size`
```C
int intro_size(const IntroType * type);
```
Return the size of a type in bytes.

# attribute information

**NOTE:** This section assumes you understand attributes. Please read the [attribute documentation](ATTRIBUTE.md).

### `intro_attribute_flag`
```C
bool intro_attribute_flag(const IntroMebmer * member, int32_t attribute);
```
Return true if `member` has the attribute `attribute`.

**example:**
```C
for (int m_index=0; m_index < type->i_struct->count_members; m_index++) {
    const IntroMember * member = &type->i_struct->members[m_index];
    if (intro_attribute_flag(member, MY_ATTR_SPECIAL)) {
        printf("%s is special!\n", member->name);
    }
}
```

### `intro_attribute_int`
```C
bool intro_attribute_int(const IntroMember * member, int32_t attribute, int32_t * o_int);
```
If `member` contains the attribute `attribute`, write the attribute's value as an int to `o_int` and return true. Otherwise, return false.    
The data written to `o_int` differs depending on the value type of the attribute:

|value type|content      | example attribute | example usage where `o_int == &i` |
|----------|-------------|-------------------|-------------------------------|
| int      |an integer   | `I(id 5)`         |`i`                            |
| member   |index into `IntroType.i_struct->members`|`I(length count)`|`type->i_struct->members[i]`   |
| string   |index into `__intro_notes`              |`I(note "hello")`|`INTRO_CTX->notes[i]`          |
| value    |index intro `__intro_values`            |`I(default 5)`   |`&INTRO_CTX->values[i]`        |

**example:**
```C
for (int m_index=0; m_index < type->i_struct->count_members; m_index++) {
    const IntroMember * member = &type->i_struct->members[m_index];
    int32_t friend_index;
    if (intro_attribute_int(member, MY_ATTR_FRIEND, &friend_index)) {
        const IntroMember * friend_member = &type->i_struct->members[friend_index];
        printf("%s has a friend: %s\n", member->name, friend_member->name);
    }
}
```

### `intro_attribute_length`
```C
bool intro_attribute_length(const void * struct_data, const IntroType * type, const IntroMember * member, int64_t * o_length);
```
If `member` has a [length](#./ATTRIBUTE.md#length) attribute, write the length to `o_length` and return true. Otherwise, return false.

**example:**
```C
Object obj = create_object();
IntroType * type = ITYPE(Object);
for (int m_index=0; m_index < type->i_struct->count_members; m_index++) {
    const IntroMember * member = &type->i_struct->members[m_index];
    if (member->type->category == INTRO_S32) {
        int64_t length;
        if (intro_attribute(&obj, type, member, &length)) {
            printf("%s has ints: ");
            int32_t * arr = *(int32_t **)(&obj + member->offset);
            for (int i=0; i < length; i++) {
                printf("%i ", arr[i]);
            }
            printf("\n");
        }
    }
}
```

### `intro_attribute_float`
```C
bool intro_attribute_float(const IntroMember * member, int32_t attribute, float * o_float);
```
If `member` has the attribute `attribute`, write the attribute's value as a float to `o_float` and return true. Otherwise, return false.    
This is only valid if the value type of `attribute` is [float](#./ATTRIBUTE.md#float).

**example:**
```C
for (int m_index=0; m_index < type->i_struct->count_members; m_index++) {
    const IntroMember * member = &type->i_struct->members[m_index];
    float scale;
    if (intro_attribute_float(member, MY_ATTR_SCALE, &scale)) {
        printf("%s has scale %f\n", member->name, scale);
    }
}
```

### `intro_set_defaults`
```C
void intro_set_defaults(void * dest, const IntroType * type);
```
Set all of the members in a struct to the value specified by the [default][attr_default] attribute or `0` if there is no specified default. This is equivilant to `intro_set_values(dest, type, INTRO_ATTR_DEFAULT)`.

**example:**
```C
Object obj;
intro_set_defaults(&obj, ITYPE(Object));
```
**See also:** [intro\_set\_values](#intro_set_values)

### `intro_set_values`
```C
void intro_set_values(void * dest, const IntroType * type, int attribute);
```
Set all the members in a struct to the value specified by the value attribute `attribute` or `0` if there is no specified value.

**example:**
```C
Object obj;
intro_set_values(&obj, ITYPE(Object), MY_ATTR_VALUE);
```
**See also:** [intro\_set\_defaults](#intro_set_defaults)

### `intro_set_member_value`
```C
void intro_set_member_value(void * dest_struct, const IntroType * struct_type, int member_index, int attribute);
```
Set the value of a single member specified by `member_index` in the struct `dest_struct`. Used internally by [intro\_set\_values](#intro_set_values).

**example:**
```C
Object obj;
intro_set_defaults(&obj, ITYPE(Object));
intro_set_member_value(&obj, ITYPE(Object), 2, MY_ATTR_VALUE);
```
**See also:** [intro\_set\_values](#intro_set_values), [intro\_set\_defaults](#intro_set_defaults)

# printers

### `intro_print`
```C
void intro_print(void * data, const IntroType * type, IntroPrintOptions * opt);
```
Print the contents of `data` to stdout using type information in `type`. This is mostly for example. I don't really know why someone would use this seriously.    
Pass `NULL` to `opt` for default options. More options will be implemented eventually.

**example:**
```C
Object obj = create_object();
intro_print(&obj, ITYPE(Object), NULL);
```

### `intro_print_type_name`
```C
void intro_print_type_name(const IntroType * type);
```
Print a textual representation of `type` to to stdout. The format is similar to how Jai or Odin represents types, which is different from C, so don't use this for metaprogramming.

**example:**
```C
typedef char (*MyType)[8];
/* ... */
intro_print_type_name(ITYPE(MyType)); // result: *[8]char
```

### `intro_sprint_type_name`
```C
void intro_sprint_type_name(char * dest, const IntroType * type);
```
Functionally identical to [intro\_print\_type\_name](#intro_print_type_name) expect output is written to `dest`.

[attr_default]: ./ATTRIBUTE.md#default