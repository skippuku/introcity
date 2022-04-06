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
    Basic obj = {0};
    obj.name = "Steven";
    obj.a = 25;
    obj.b = -53;
    for (int i=0; i < 8; i++) {
        obj.array[i] = i * i;
    }

    IntroType * obj_type = get_type_with_name("Basic");
    void * city_data;
    int city_size = city_create(&city_data, &obj, obj_type);

    int error = dump_to_file("test/obj.cty", city_data, city_size);

    return error;
}
