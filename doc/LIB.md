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

### `intro_create_city_file`
```C
bool intro_create_city_file(const char * filename, void * src, const IntroType * src_type);
```
Convenience function that creates city data and writes it to a file. Returns false on failure and true on sucess.

### `intro_load_city_file`
```C
void * intro_load_city_file(void * dest, const IntroType * dest_type, const char * filename);
```
Convenience function that loads city data from a file. Returns a pointer to the data that should be freed when the object pointed to by `dest` is no longer in use. Returns `NULL` on failure.

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
INTRO_INLINE int intro_size(const IntroType * type);
```
Return the size of a type in bytes.

### `intro_origin`
```C
INTRO_INLINE const IntroType * intro_origin(const IntroType * type);
```
Follow typedefs until the original type is found, then return that type.

# attribute information

**NOTE:** This section assumes you understand attributes. Please read the [attribute documentation](ATTRIBUTE.md).
**ALSO NOTE:** These are all macros. Use the name of the attribute *with its namespace* to represent the attribute. This gets internally expanded to an enum value.

### `intro_has_attribute`
```C
bool intro_attribute_flag(const IntroMebmer * member, attribute);
```
Return true if `member` has the attribute `attribute`.

**example:**
```C
for (int m_index=0; m_index < type->i_struct->count_members; m_index++) {
    const IntroMember * member = &type->i_struct->members[m_index];
    if (intro_has_attribute(member, my_attr_special)) {
        printf("%s is special!\n", member->name);
    }
}
```

### `intro_attribute_value`
```C
bool intro_attribute_value(const IntroMember * member, attribute, IntroVariant * o_var);
```
If `member` has the specified `value` attribute, write the value to o\_var and return true. Otherwise return false.

**example:**
```C
IntroVariant speed_var;
if (intro_attribute_value(member, i_default, &speed_var) {
    auto speed = intro_var_get(speed_var, float);
}
```

### `intro_attribute_int`
```C
bool intro_attribute_int(const IntroMember * member, attribute, int32_t * o_int);
```
If `member` contains the specified `int` attribute, write the attribute's value as an int to `o_int` and return true. Otherwise, return false.    

### `intro_attribute_member`
```C
bool intro_attribute_member(const IntroMember * member, attribute, int32_t * o_member_index);
```
If `member` has the specified `member` attribute, write the member index to `o_member_index` and return true. Otherwise return false.

**example:**  
```C
for (int m_index=0; m_index < type->i_struct->count_members; m_index++) {
    const IntroMember * member = &type->i_struct->members[m_index];
    int32_t friend_index;
    if (intro_attribute_member(member, my_attr_friend, &friend_index)) {
        const IntroMember * friend_member = &type->i_struct->members[friend_index];
        printf("%s has a friend: %s\n", member->name, friend_member->name);
    }
}
```

### `intro_attribute_float`
```C
bool intro_attribute_float(const IntroMember * member, int32_t attribute, float * o_float);
```
If `member` has the specified `float` attribute, write the attribute's value as a float to `o_float` and return true. Otherwise, return false.    
This is only valid if the value type of `attribute` is [float](#./ATTRIBUTE.md#float).

**example:**
```C
for (int m_index=0; m_index < type->i_struct->count_members; m_index++) {
    const IntroMember * member = &type->i_struct->members[m_index];
    float scale;
    if (intro_attribute_float(member, gui_attr_scale, &scale)) {
        printf("%s has scale %f\n", member->name, scale);
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
