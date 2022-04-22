#include "../lib/intro.h"

#include "test.h"
#include "test.h.intro"

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
    obj_save.wood_type = WOOD_CHERRY;
    obj_save.stuff.a = 95;
    obj_save.stuff.b = -95;

    printf("obj_save: Basic = ");
    intro_print(&obj_save, obj_save_type, NULL);
    printf("\n\n");

    intro_create_city_file("test/obj.cty", &obj_save, obj_save_type);

    BasicPlus obj_load;
    intro_load_city_file(&obj_load, ITYPE(BasicPlus), "test/obj.cty");

    printf("obj_load: BasicPlus = ");
    intro_print(&obj_load, obj_load.type, NULL);
    printf("\n");

    return 0;
}
