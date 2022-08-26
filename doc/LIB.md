# intro library reference
**NOTE:** *intro/city* is in beta. API is subject to change.    

# CITY API

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

# Type information

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

### `intro_has_members`
```C
INTRO_INLINE bool intro_has_members(const IntroType * type);
```
Return true if `type` is a struct or union.

### `intro_has_of`
```C
INTRO_INLINE bool intro_has_of(const IntroType * type);
```
Return true if `type` is an array or a pointer.

### `intro_is_complex`
```C
INTRO_INLINE bool intro_is_complex(const IntroType * type);
```
Return true if `type` is a struct, union, or enum.

### `intro_origin`
```C
INTRO_INLINE const IntroType * intro_origin(const IntroType * type);
```
Follow typedefs until the original type is found, then return that type.

# Container API

Containers are used to preserve context when working on data.

### `intro_cntr`
```C
IntroContainer intro_container(void * data, const IntroType * type);
```
Create a container with `data` and `type`.

### `intro_push`
```C
IntroContainer intro_push(const IntroContainer * parent, int32_t index);
```
Create a child container with `parent`. `index` is the member index if `parent` is a struct or union, or it is the array index for pointers and arrays.

### `intro_get_attr`
```C
uint32_t intro_get_attr(IntroContainer cntr);
```
Return the attribute specification value for `cntr`.

## `intro_get_member`
```C
const IntroMember * intro_get_member(IntroContainer cntr);
```
If `cntr` represents a member of a struct or union, return the `IntroMember` information for that member. Otherwise, return NULL.

# Attribute information

**NOTE:** This section assumes you understand attributes. Please read the [attribute documentation](ATTRIBUTE.md).
**ALSO NOTE:** These are all macros. Use the name of the attribute type *with its namespace* to represent the attribute. This gets internally expanded to an enum value.

### `intro_has_attribute`
```C
bool intro_has_attribute(const IntroMebmer * member, IntroAttributeType attr_type);
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
bool intro_attribute_value(const IntroMember * member, IntroAttributeType attr_type, IntroVariant * o_var);
```
If `member` has the specified `value` attribute, write the value to o\_var and return true. Otherwise return false.  
If the attribute type is not a pointer, var.data will point to the value. If the attribute type is a pointer, var.data will be the value itself.

**example:**
```C++
IntroVariant var;
if (intro_attribute_value(member, i_default, &var)) {
    auto speed = intro_var_get(var, float);
}

if (intro_attriute_value(member, gui_note, &var)) {
    auto speed = (char *)var.data;
}
```

### `intro_attribute_int`
```C
bool intro_attribute_int(const IntroMember * member, IntroAttributeType attr_type, int32_t * o_int);
```
If `member` contains the specified `int` attribute, write the attribute's value as an int to `o_int` and return true. Otherwise, return false.    

### `intro_attribute_member`
```C
bool intro_attribute_member(const IntroMember * member, IntroAttributeType attr_type, int32_t * o_member_index);
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
bool intro_attribute_float(const IntroMember * member, IntroAttributeType attr_type, float * o_float);
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
bool intro_attribute_length(IntroContainer cntr, int64_t * o_length);
```
If `cntr` has a [length](#./ATTRIBUTE.md#length) attribute, write the length to `o_length` and return true. Otherwise, return false.

**example:**
```C
Object obj = create_object();
IntroContainer struct_cntr = intro_container(&obj, ITYPE(Object));

for (int m_index=0; m_index < ITYPE(Object)->count; m_index++) {
    IntroContainer m_cntr = intro_push(&struct_cntr, m_index);
    IntroMember member = ITYPE(Object)->members[m_index];
    if (m_cntr.type->category == INTRO_POINTER && m_cntr.type->of->category == INTRO_S32) {
        int64_t length;
        if (intro_attribute_length(m_cntr, &length)) {
            printf("%s has %i ints: ", member.name, (int)length);
            int32_t * arr = *(int32_t **)(&obj + member.offset);
            for (int i=0; i < length; i++) {
                printf("%i ", arr[i]);
            }
            printf("\n");
        }
    }
}
```

### `intro_attribute_run_expr`
```C
bool intro_attribute_run_expr(IntroContainer cntr, IntroAttributeType attr_type, int64_t * o_result)
```
If `cntr` has an attribute of type `attr_type` which is of the category [expr](#./ATTRIBUTE.md#expr), run the expression and write the result to `o_result` and return true. Otherwise, return false.

### `intro_fallback`
```C
void intro_fallback(void * dest, const IntroType * type);
```
Set dest to its 'fallback' value.

**example:**
```C
Object obj;
intro_fallback(&obj, ITYPE(Object));
```
**See also:** [intro\_set\_value](#intro_set_value)

### `intro_set_value`
```C
void intro_set_value(void * dest, const IntroType * type, IntroAttributeType attr_type);
```
Set the data in `dest` to the value specified with `attr_type`.

**example:**
```C
Object obj;
intro_set_value(&obj, ITYPE(Object), my_value);
```

# Printers

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
