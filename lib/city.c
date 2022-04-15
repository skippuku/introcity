#include "lib.c"

#define city_load(dest, dest_type, data, data_size) city_load_ctx(INTRO_CTX, dest, dest_type, data, data_size)

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

static uint32_t
city__get_serialized_id(CityCreationContext * ctx, const IntroType * type) {
    int type_id = hmgeti(ctx->type_set, type);
    if (type_id >= 0) {
        return type_id;
    }

    if (intro_is_scalar(type)) {
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
static void
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

void *
city_create(const void * src, const IntroType * s_type, size_t *o_size) {
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

    *o_size = arrlen(result);
    return (void *)result;
}

static uint32_t
next_uint(const uint8_t ** ptr, uint8_t size) {
    uint32_t result = 0;
    //memcpy(&result + 4 - size, *ptr, size); // BE to BE
    memcpy(&result, *ptr, size); // LE to LE
    *ptr += size;
    return result;
}

static int
city__safe_copy_struct(
    IntroContext * ctx,
    void * restrict dest,
    const IntroType * restrict d_type,
    void * restrict src,
    const IntroType * restrict s_type
) {
    const IntroStruct * d_struct = d_type->i_struct;
    const IntroStruct * s_struct = s_type->i_struct;
    for (int i=0; i < d_struct->count_members; i++) {
        const IntroMember * dm = &d_struct->members[i];

        if (intro_attribute_flag(dm, INTRO_ATTR_TYPE)) {
            *(const IntroType **)(dest + dm->offset) = d_type;
            continue;
        }

        const char ** aliases = NULL;
        arrput(aliases, dm->name);
        for (int attr_i=0; attr_i < dm->count_attributes; attr_i++) {
            if (dm->attributes[attr_i].type == INTRO_ATTR_ALIAS) {
                const char * alias = ctx->notes[dm->attributes[attr_i].v.i];
                arrput(aliases, alias);
            }
        }

        bool found_match = false;
        for (int j=0; j < s_struct->count_members; j++) {
            const IntroMember * sm = &s_struct->members[j];

            bool match = false;
            for (int ai=0; ai < arrlen(aliases); ai++) {
                if (strcmp(aliases[ai], sm->name) == 0) {
                    match = true;
                    break;
                }
            }
            if (match) {
                if (dm->type->category != sm->type->category) {
                    city__error("fatal type mismatch");
                    return -1;
                }

                if (intro_is_scalar(dm->type)) {
                    memcpy(dest + dm->offset, src + sm->offset, intro_size(dm->type));
                } else if (dm->type->category == INTRO_POINTER) {
                    void * result_ptr = src + *(size_t *)(src + sm->offset);
                    memcpy(dest + dm->offset, &result_ptr, sizeof(void *));
                } else if (dm->type->category == INTRO_ARRAY) {
                    if (dm->type->parent->category != sm->type->parent->category) {
                        city__error("array type mismatch");
                        return -1;
                    }
                    size_t d_size = intro_size(dm->type);
                    size_t s_size = intro_size(sm->type);
                    size_t size = (d_size > s_size)? s_size : d_size;
                    memcpy(dest + dm->offset, src + sm->offset, size);
                } else if (dm->type->category == INTRO_STRUCT) {
                    int ret = city__safe_copy_struct(ctx,
                        dest + dm->offset, dm->type,
                        src + sm->offset, sm->type
                    );
                    if (ret < 0) return ret;
                } else {
                    return -1;
                }
                found_match = true;
                break;
            }
        }
        if (!found_match) {
            int32_t value_offset;
            if (intro_attribute_int(dm, INTRO_ATTR_DEFAULT, &value_offset)) {
                memcpy(dest + dm->offset, ctx->values + value_offset, intro_size(dm->type));
            } else if (dm->type->category == INTRO_STRUCT) {
                intro_set_defaults_ctx(ctx, dest + dm->offset, dm->type);
            } else {
                memset(dest + dm->offset, 0, intro_size(dm->type));
            }
        }
        arrfree(aliases);
    }

    return 0;
}

int
city_load_ctx(IntroContext * ctx, void * dest, const IntroType * d_type, void * data, int32_t data_size) {
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

    struct {
        uint32_t key;
        IntroType * value;
    } * info_by_id = NULL;

    uint8_t * src = data + header->data_ptr;
    const uint8_t * b = data + sizeof(*header);

    for (int i=0; i < header->count_types; i++) {
        IntroType * type = malloc(sizeof(*type));
        memset(type, 0, sizeof(*type));

        type->category = next_uint(&b, 1);

        switch(type->category) {
        case INTRO_STRUCT:
        case INTRO_UNION: {
            uint32_t count_members = next_uint(&b, 4);

            if ((void *)b + count_members * (type_size + offset_size + offset_size) > data + data_size) {
                city__error("malformed");
                return -1;
            }

            IntroMember * members = NULL;
            for (int m=0; m < count_members; m++) {
                IntroMember member;
                member.type   = hmget(info_by_id, next_uint(&b, type_size));
                member.offset = (int32_t)next_uint(&b, offset_size);
                member.name   = (char *)(src + next_uint(&b, offset_size));
                arrput(members, member);
            }

            IntroStruct struct_;
            struct_.count_members = count_members;
            struct_.is_union = type->category == INTRO_UNION;

            IntroStruct * result = malloc(sizeof(IntroStruct) + sizeof(IntroMember) * arrlen(members));
            memcpy(result, &struct_, sizeof(struct_));
            memcpy(result->members, members, sizeof(*members) * arrlen(members));
            arrfree(members);

            type->i_struct = result;
        }break;

        case INTRO_POINTER: {
            uint32_t parent_id = next_uint(&b, type_size);

            IntroType * parent = hmget(info_by_id, parent_id);
            type->parent = parent;
        }break;

        case INTRO_ARRAY: {
            uint32_t parent_id = next_uint(&b, type_size);
            uint32_t array_size = next_uint(&b, 4);

            IntroType * parent = hmget(info_by_id, parent_id);
            type->parent = parent;
            type->array_size = array_size;
        }break;

        default: break;
        }
        hmput(info_by_id, i, type);
    }

    const IntroType * s_type = info_by_id[hmlen(info_by_id) - 1].value;

    int copy_result = city__safe_copy_struct(ctx, dest, d_type, src, s_type);

    for (int i=0; i < hmlen(info_by_id); i++) {
        free(info_by_id[i].value);
    }
    hmfree(info_by_id);

    return copy_result;
}
