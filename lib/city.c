#include "../intro.h"

bool
intro_is_basic(const IntroType * type) {
    return (type->category >= INTRO_U8 && type->category <= INTRO_F64);
}

int
intro_size(const IntroType * type) {
    if (intro_is_basic(type)) {
        return (type->category & 0x0f);
    } else if (type->category == INTRO_POINTER) {
        return sizeof(void *);
    } else if (type->category == INTRO_ARRAY) {
        return type->array_size * intro_size(type->parent);
    } else if (type->category == INTRO_STRUCT || type->category == INTRO_UNION) {
        return type->i_struct->size;
    } else if (type->category == INTRO_ENUM) {
        return type->i_enum->size;
    } else {
        return 0;
    }
}

int64_t
intro_int_value(const void * data, const IntroType * type) {
    int64_t result;
    switch(type->category) {
    case INTRO_U8:
        result = *(uint8_t *)data;
        break;
    case INTRO_U16:
        result = *(uint16_t *)data;
        break;
    case INTRO_U32:
        result = *(uint32_t *)data;
        break;
    case INTRO_U64:
        result = *(uint64_t *)data;
        break;

    case INTRO_S8:
        result = *(int8_t *)data;
        break;
    case INTRO_S16:
        result = *(int16_t *)data;
        break;
    case INTRO_S32:
        result = *(int32_t *)data;
        break;
    case INTRO_S64:
        result = *(int64_t *)data;
        break;

    default:
        result = 0;
        break;
    }

    return result;
}

bool
intro_attribute_int(const IntroMember * m, int32_t attr_type, int32_t * o_int) {
    for (int i=0; i < m->count_attributes; i++) {
        const IntroAttributeData * attr = &m->attributes[i];
        if (attr->type == attr_type) {
            if (o_int) *o_int = attr->v.i;
            return true;
        }
    }
    return false;
}

bool
intro_attribute_length(const void * struct_data, const IntroType * struct_type, const IntroMember * m, int64_t * o_length) {
    int32_t member_index;
    const void * m_data = struct_data + m->offset;
    if (intro_attribute_int(m, INTRO_ATTR_LENGTH, &member_index)) {
        const IntroMember * length_member = &struct_type->i_struct->members[member_index];
        const void * length_member_loc = struct_data + length_member->offset;
        *o_length = intro_int_value(length_member_loc, length_member->type);
        return true;
    } else {
        *o_length = 0;
        return false;
    }
}

static const int implementation_version_major = 0;
static const int implementation_version_minor = 1;

typedef struct {
    char magic_number [4];
    uint16_t version_major;
    uint16_t version_minor;
    uint8_t  size_info;
    uint8_t  reserved_0 [3];
    uint32_t data_ptr;
    uint32_t count_types;
} CityHeader;

static void
city__error(const char * msg) {
    fprintf(stderr, "error: %s\n", msg);
}

static void
put_uint(uint8_t ** array, uint32_t number, uint8_t bytes) {
    memcpy(arraddnptr(*array, bytes), &number, bytes);
}

#if 0
Quad
swipe_quad(Quad q, vec2 a, vec2 b) {
    vec2 t_tl = {MIN(a.x, b.x), MIN(a.y, b.y)};
    vec2 t_br = {MAX(a.x, b.x), MAX(a.y, b.y)};

    Quad result = {addv2(t_tl, q.tl), addv2(t_br, q.br)};
    return result;
}
#endif

typedef struct {
    const IntroType * key;
} CityTypeSet;

typedef struct {
    void * key;
    uint32_t value;
} U32ByPtr;

typedef struct {
    uint8_t type_size;
    uint8_t offset_size;
    CityTypeSet * type_set;
    uint8_t * info;
    uint8_t * data;

    U32ByPtr * data_offset_by_ptr;
} CityCreationContext;

uint32_t
city__get_serialized_id(CityCreationContext * ctx, const IntroType * type) {
    int type_id = hmgeti(ctx->type_set, type);
    if (type_id >= 0) {
        return type_id;
    }

    if (intro_is_basic(type)) {
        put_uint(&ctx->info, type->category, 1);
    } else {
        switch(type->category) {
        case INTRO_ARRAY: {
            uint32_t parent_type_id = city__get_serialized_id(ctx, type->parent);
            put_uint(&ctx->info, type->category, 1);
            put_uint(&ctx->info, parent_type_id, ctx->type_size);
            put_uint(&ctx->info, type->array_size, 4);
        }break;

        case INTRO_POINTER: {
            uint32_t parent_type_id = city__get_serialized_id(ctx, type->parent);
            put_uint(&ctx->info, type->category, 1);
            put_uint(&ctx->info, parent_type_id, ctx->type_size);
        }break;

        case INTRO_ENUM: {
            put_uint(&ctx->info, type->i_enum->size, 1);
        }break;

        case INTRO_UNION:
        case INTRO_STRUCT: {
            const IntroStruct * s_struct = type->i_struct;
            uint32_t m_type_ids [s_struct->count_members];
            for (int m_index=0; m_index < s_struct->count_members; m_index++) {
                const IntroMember * m = &s_struct->members[m_index];
                m_type_ids[m_index] = city__get_serialized_id(ctx, m->type);
            }

            put_uint(&ctx->info, type->category, 1);
            put_uint(&ctx->info, s_struct->count_members, 4);
            for (int m_index=0; m_index < s_struct->count_members; m_index++) {
                const IntroMember * m = &s_struct->members[m_index];
                put_uint(&ctx->info, m_type_ids[m_index], ctx->type_size);

                put_uint(&ctx->info, m->offset, ctx->offset_size);

                size_t m_name_len = strlen(m->name);
                uint32_t name_offset = arrlen(ctx->data);
                memcpy(arraddnptr(ctx->data, m_name_len + 1), m->name, m_name_len + 1);
                put_uint(&ctx->info, name_offset, ctx->offset_size);
            }
        }break;

        default: break;
        }
    }

    hmputs(ctx->type_set, (CityTypeSet){type});
    type_id = hmtemp(ctx->type_set);
    return type_id;
}

// TODO: support ptr to ptr
void
city__serialize_pointer_data(CityCreationContext * ctx, const IntroType * s_type) {
    assert(s_type->category == INTRO_STRUCT);

    for (int m_index=0; m_index < s_type->i_struct->count_members; m_index++) {
        const IntroMember * member = &s_type->i_struct->members[m_index];

        if (member->type->category == INTRO_POINTER) {
            void ** o_ptr = (void **)(ctx->data + member->offset);
            void * ptr = *o_ptr;
            if (!ptr) continue;

            if (hmgeti(ctx->data_offset_by_ptr, ptr) >= 0) {
                *o_ptr = (void *)(size_t)hmget(ctx->data_offset_by_ptr, ptr);
                continue;
            }

            int64_t length;
            if (intro_attribute_length(ctx->data, s_type, member, &length)) {
            } else if (member->type->parent->name && strcmp(member->type->parent->name, "char") == 0) {
                length = strlen((char *)ptr) + 1;
            } else {
                length = 1;
            }
            size_t alloc_size = intro_size(member->type->parent) * length;
            uint32_t * ser_length = (uint32_t *)arraddnptr(ctx->data, 4);
            *ser_length = length;

            void * ser_data = arraddnptr(ctx->data, alloc_size);
            memcpy(ser_data, ptr, alloc_size);

            uint32_t offset = ser_data - (void *)ctx->data;

            hmput(ctx->data_offset_by_ptr, ptr, offset);

            o_ptr = (void **)(ctx->data + member->offset);
            *o_ptr = (void *)(size_t)offset;
        }
    }
}

int32_t // size
city_create(void ** o_result, const void * src, const IntroType * s_type) {
    assert(s_type->category == INTRO_STRUCT);
    const IntroStruct * s_struct = s_type->i_struct;

    CityHeader header = {0};
    memcpy(header.magic_number, "ICTY", 4);
    header.version_major = implementation_version_major;
    header.version_minor = implementation_version_minor;

    CityCreationContext ctx_ = {0}, *ctx = &ctx_;

    // TODO: base on actual data size
    ctx->type_size = 2;
    ctx->offset_size = 3;
    header.size_info = ((ctx->type_size-1) << 4) | (ctx->offset_size-1);

    void * src_cpy = arraddnptr(ctx->data, s_struct->size);
    memcpy(src_cpy, src, s_struct->size);

    uint32_t main_type_id = city__get_serialized_id(ctx, s_type);
    uint32_t count_types = hmlenu(ctx->type_set);
    assert(main_type_id == count_types - 1);

    header.count_types = count_types;
    header.data_ptr = sizeof(header) + arrlen(ctx->info);

    city__serialize_pointer_data(ctx, s_type);

    uint8_t * result = NULL;
    memcpy(arraddnptr(result, sizeof(header)), &header, sizeof(header));

    memcpy(arraddnptr(result, arrlen(ctx->info)), ctx->info, arrlen(ctx->info));
    memcpy(arraddnptr(result, arrlen(ctx->data)), ctx->data, arrlen(ctx->data));

    arrfree(ctx->info);
    arrfree(ctx->data);
    hmfree(ctx->type_set);

    *o_result = (void *)result;
    return arrlen(result);
}

#ifdef IGNORE
static uint32_t
next_uint(const uint8_t ** ptr, uint8_t size) {
    uint32_t result;
    //memcpy(&result + 4 - size, *ptr, size); // BE to BE
    memcpy(&result, *ptr, size); // LE to LE
    *ptr += size;
    return result;
}

static int
city__safe_copy_struct(
    void * restrict dest,
    const IntroType * restrict d_type,
    const void * restrict src,
    const s_type * restrict s_type
) {
    const IntroStruct * d_struct = d_type->i_struct;
    const IntroStruct * s_struct = s_type->i_struct;
    for (int i=0; i < d_struct->count_members; i++) {
        const IntroMember * dm = &d_struct->members[i];

        for (int j=0; j < s_struct->count_members; j++) {
            const IntroMember * sm = &s_struct->members[j];

            if (strcmp(dm->name, sm->name) == 0) {
                if (dm->type->category != sm->type->category) {
                    city__error("fatal type mismatch");
                    return -1;
                }

                if (intro_is_basic(dm->type->category)) {
                    memcpy(dest + dm->offset, src + sm->offset, intro_sizeof(dm->type));
                } else if (dm->type->category == INTRO_POINTER) {
                    void * result_ptr = data + *(src + sm->offset);
                    memcpy(dest + dm->offset, &result_ptr, sizeof(void *));
                } else if (dm->type->category == INTRO_STRUCT) {
                    int ret = city__safe_copy_struct(
                        dest + dm->offset, dm->type->i_struct
                        src + sm->offset, sm->type->i_struct
                    );
                    if (ret < 0) return ret;
                } else {
                    return -1;
                }
            }
        }
    }

    return 0;
}

int
city_load(void * dest, IntroType * d_type, const void * data, int32_t data_size) {
    const CityHeader * header = data;
    
    if (
        data_size < sizeof(*header)
     || memcmp(header->magic_number, "ICTY", 4) != 0
    ) {
        city__error("invalid CTY file");
        return -1;
    }

    if (header->version_major != implementation_version_major) {
        city__error("unsupported CTY version.");
        return -1;
    }

    if (header->version_minor > implementation_version_minor) {
        city__error("warning: some features will be unsupported.");
    }

    const uint8_t type_size   = 1 + ((header->size_info >> 4) & 0x0f);
    const uint8_t offset_size = 1 + ((header->size_info) & 0x0f) ;

    const uint8_t * b = data + sizeof(*header);
    if (b + header->count_types * (type_size + offset_size) < data + data_size) {
        city__error("malformed");
        return -1;
    }

    struct {
        uint32_t key;
        IntroType value;
    } * info_by_type = NULL;

    for (int i=0; i < header->count_types; i++) {
        uint32_t type = next_uint(&b, type_size);
        uint32_t offset = next_uint(&b, offset_size);

        IntroType type;
        const uint8_t * p = data + offset;

        type.category = next_uint(&p, 1);

        switch(info.category) {
        case INTRO_STRUCT:
        case INTRO_UNION: {
            uint32_t count_members = next_uint(&p, 4);

            if (p + count_members * (type_size + offset_size + offset_size) < data + data_size) {
                city__error("malformed");
                return -1;
            }

            IntroMember * members = NULL;
            for (int m=0; m < count_members; m++) {
                IntroMember member;
                member.type   = hmgetp_null(next_uint(&p, type_size));
                member.offset = (int32_t)next_uint(&p, offset_size);
                member.name   = (const char *)(data + next_uint(&p, offset_size));
                arrput(members, member);
            }

            IntroStruct struct_;
            struct_.name = NULL;
            struct_.count_members = count_members;
            struct_.is_union = info.category == INTRO_UNION;

            IntroStruct * result = malloc(sizeof(IntroStruct) + sizeof(IntroMember) * arrlen(members));
            memcpy(result, &struct_, sizeof(struct_));
            memcpy(result->members, members, sizeof(*members) * arrlen(members));
            arrfree(members);

            IntroType type_info = {0};
            type_info.category = INTRO_STRUCT;
            type_info.i_struct = result;

            hmput(info_by_type, type, type_info);
        } break;

        case INTRO_POINTER: {
            uint32_t to_type = next_uint(&p, type_size);

            IntroType type_info = hmget(info_by_type, to_type);
            uint32_t * ind = malloc(type_info.indirection_level + 1);
            ind[0] = 0;
            for (int i=0; i < type_info.indirection_level; i++) {
                ind[i+1] = type_info.indirection[i];
            }
            type_info.indirection = ind;
            type_info.indirection_level++;

            hmput(info_by_type, type, type_info);
        } break;

        case INTRO_ARRAY: {
            uint32_t type = next_uint(&p, type_size);
            uint32_t array_size = next_uint(&p, 4);

            IntroType type_info = hmget(info_by_type, to_type);
            uint32_t * ind = malloc(type_info.indirection_level + 1);
            ind[0] = array_size;
            for (int i=0; i < type_info.indirection_level; i++) {
                ind[i+1] = type_info.indirection[i];
            }
            type_info.indirection = ind;
            type_info.indirection_level++;

            hmput(info_by_type, type, type_info);
        } break;

        }
    }

    const void * src = data + header->data_ptr;
    const IntroType * s_type = &arrlast(info_by_type);

    return city__safe_copy_struct(dest, d_type, src, s_type);
}
#endif
