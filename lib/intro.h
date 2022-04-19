#ifndef INTRO_H
#define INTRO_H

#include "types.h"

#ifndef INTRO_CTX
#define INTRO_CTX &__intro_ctx
#endif

#define intro_set_defaults(dest, type) intro_set_defaults_ctx(INTRO_CTX, dest, type)
#define intro_set_values(dest, type, attribute) intro_set_values_ctx(INTRO_CTX, dest, type, attribute)
#define intro_type_with_name(name) intro_type_with_name_ctx(INTRO_CTX, name)
#define intro_print(data, type, opt) intro_print_ctx(INTRO_CTX, data, type, opt)

#define intro_load_city(dest, dest_type, data, data_size) intro_load_city_ctx(INTRO_CTX, dest, dest_type, data, data_size)

#ifndef INTRO_INLINE
#define INTRO_INLINE static inline
#endif

INTRO_INLINE bool
intro_is_scalar(const IntroType * type) {
    return (type->category >= INTRO_U8 && type->category <= INTRO_F64);
}

INTRO_INLINE bool
intro_is_int(const IntroType * type) {
    return (type->category >= INTRO_U8 && type->category <= INTRO_S64);
}

INTRO_INLINE bool
intro_is_complex(const IntroType * type) {
    return (type->category == INTRO_STRUCT
         || type->category == INTRO_UNION
         || type->category == INTRO_ENUM);
}

typedef struct IntroNameSize {
    char * name;
    size_t size;
} IntroNameSize;

typedef struct {
    int indent;
} IntroPrintOptions;

int intro_size(const IntroType * type);
const IntroType * intro_base(const IntroType * type, int * o_depth);
int64_t intro_int_value(const void * data, const IntroType * type);
bool intro_attribute_flag(const IntroMember * m, int32_t attr_type);
bool intro_attribute_int(const IntroMember * m, int32_t attr_type, int32_t * o_int);
bool intro_attribute_float(const IntroMember * m, int32_t attr_type, float * o_float);
bool intro_attribute_length(const void * struct_data, const IntroType * struct_type, const IntroMember * m, int64_t * o_length);
void intro_set_member_value_ctx(IntroContext * ctx, void * dest, const IntroType * struct_type, int member_index, int value_attribute);
void intro_set_values_ctx(IntroContext * ctx, void * dest, const IntroType * type, int value_attribute);
void intro_set_defaults_ctx(IntroContext * ctx, void * dest, const IntroType * type);
void * intro_joint_alloc(void * dest, const IntroType * type, const IntroNameSize * list, size_t count);
void intro_sprint_type_name(char * dest, const IntroType * type);
void intro_print_type_name(const IntroType * type);
void intro_print_ctx(IntroContext * ctx, const void * data, const IntroType * type, const IntroPrintOptions * opt);
IntroType * intro_type_with_name_ctx(IntroContext * ctx, const char * name);

void * intro_create_city(const void * src, const IntroType * s_type, size_t *o_size);
int intro_load_city_ctx(IntroContext * ctx, void * dest, const IntroType * d_type, void * data, int32_t data_size);

#endif // INTRO_H
