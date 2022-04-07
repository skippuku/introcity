#include "../intro.h"
#include "test.h"
#include "test.h.intro"

#include "../util.c"
#include "../lib/city.c"

static IntroType *
get_type_with_name(const char * name) {
    for (int i=0; i < LENGTH(__intro_types); i++) {
        IntroType * type = &__intro_types[i];
        if (type->name && strcmp(type->name, name) == 0) {
            return type;
        }
    }
    return NULL;
}

int
main() {
    Basic obj_save = {0};
    obj_save.name = "Steven";
    obj_save.a = 25;
    obj_save.b = -53;
    for (int i=0; i < 8; i++) {
        obj_save.array[i] = i * i;
    }

    IntroType * obj_save_type = get_type_with_name("Basic");
    void * city_data;
    int city_size = city_create(&city_data, &obj_save, obj_save_type);

    int error = dump_to_file("test/obj.cty", city_data, city_size);
    if (error) {
        printf("error writing to file.\n");
        return error;
    }

    BasicPlus obj_load = {0};
    size_t load_size;
    void * city_load_data = read_entire_file("test/obj.cty", &load_size);
    city_load(&obj_load, get_type_with_name("BasicPlus"), city_load_data, load_size);

    printf("obj_load.name = \"%s\"\n", obj_load.name);
    printf("obj_load.a    = %i\n", obj_load.a);
    printf("obj_load.b    = %i\n", obj_load.b);
    printf("obj_load.c    = %i\n", obj_load.c);
    printf("obj_load.array = {");
    for (int i=0; i < 8; i++) {
        printf("%hhu, ", obj_load.array[i]);
    }
    printf("}\n");
    printf("obj_load.has_c = %u\n", obj_load.has_c);

    return 0;
}
