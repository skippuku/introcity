#include "../intro.h"
#include "test.h"
#include "test.h.intro"

#include "../lib/city.c"

void
dump_city(const char * filename, void * data, const IntroType * type) {
    size_t city_size;
    void * city_data = city_create(data, type, &city_size);

    int error = dump_to_file(filename, city_data, city_size);
    if (error) {
        printf("error writing to file: %s\n", filename);
        exit(1);
    }
}

void
read_city(const char * filename, void * data, const IntroType * type) {
    size_t load_size;
    void * city_load_data = read_entire_file(filename, &load_size);
    city_load(data, type, city_load_data, load_size);
}

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
    obj_save.count_numbers = 15;
    obj_save.numbers = calloc(obj_save.count_numbers, sizeof(*obj_save.numbers));
    for (int i=0; i < obj_save.count_numbers; i++) {
        obj_save.numbers[i] = (rand() % 1000);
    }

    printf("obj_save: Basic = ");
    intro_print_struct(&obj_save, obj_save_type, NULL);
    printf("\n\n");

    dump_city("test/obj.cty", &obj_save, obj_save_type);

    BasicPlus obj_load = {0};
    read_city("test/obj.cty", &obj_load, intro_type_with_name("BasicPlus"));

    printf("obj_load: BasicPlus = ");
    intro_print_struct(&obj_load, obj_load.type, NULL);
    printf("\n");

    return 0;
}
