#include "../intro.h"
#include "test.h"
#include "test.h.intro"

#include "../lib/city.c"

int
main() {
    Basic obj_save = {0};
    IntroType * obj_save_type = intro_type_with_name("Basic");
    obj_save.name = "Steven";
    obj_save.a = 25;
    obj_save.b = -53;
    for (int i=0; i < 8; i++) {
        obj_save.array[i] = i * i;
    }

    printf("obj_save: Basic = ");
    intro_print_struct(&obj_save, obj_save_type, NULL);
    printf("\n\n");

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
    city_load(&obj_load, intro_type_with_name("BasicPlus"), city_load_data, load_size);

    printf("obj_load: BasicPlus = ");
    intro_print_struct(&obj_load, obj_load.type, NULL);
    printf("\n");

    return 0;
}
