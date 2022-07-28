#ifndef __INTRO__
#include "basic.h"
#endif

#include "../lib/intro.h"

enum Wood {
    WOOD_PINE,
    WOOD_ASH,
    WOOD_BIRCH,
    WOOD_CHERRY,
    WOOD_MAHOGANY,
};

typedef struct LinkNodeSave LinkNodeSave;

struct LinkNodeSave {
    LinkNodeSave * next;
    int value;
};

typedef struct {
    int id      I(= -1);
    char * name I(= "unnamed");
} StuffSave;

typedef struct {
    uint32_t hex I(= 0xFF56A420, gui_format "0x%x");
    int id       I(= -1);
    double speed I(= 5.6);
    char * name  I(= "unnamed");
} StuffLoad;

typedef struct {
    char * name;
    int32_t a, b;
    uint8_t array [8];

    int16_t * numbers I(length count_numbers);
    int32_t count_numbers;

    enum Wood wood_type;

    struct {
        int a, b;
    } stuff;

    int cool_number I(5);
    char * character;
    char * long_member_name_that_would_take_up_a_great_deal_of_space_in_a_city_file I(id 9, = "jerry");

    int * _internal I(10, ~city);

    StuffSave * stuffs I(length count_stuffs);
    int count_stuffs;

    struct {
        char * buffer I(1);
        char * bookmark I(2, ~cstring);
    } text;

    LinkNodeSave * linked I(11);
} Basic;

typedef struct LinkNodeLoad LinkNodeLoad;
struct LinkNodeLoad {
    LinkNodeLoad * next;
    double something;
    int value;
};

typedef struct {
    char * name I(0);
    int32_t a I(1), c I(2);
    int32_t b2 I(3, alias b);
    uint8_t array [8] I(4);
    bool has_c I(7);
    IntroType * type I(type);

    struct {
        int a, b, c;
        float scale I(= 10.0);
    } stuff;

    struct {
        bool is_ok I(= 1);
        float speed I(= 2.3);
        double time_stamp;
        int64_t seconds_left;
        int32_t * a_pointer;
    } universe;

    enum Wood wood_type I(8);

    int16_t * numbers I(length count_numbers);
    int32_t count_numbers I(137);

    int cool_number I(5);
    char * character I(6);
    char * long_member_name_that_would_take_up_a_great_deal_of_space_in_a_city_file I(9);

    struct {
        char * buffer I(1);
        char * bookmark I(2, ~cstring);
    } text;

    StuffLoad * stuffs I(length count_stuffs);
    int count_stuffs;

    int * _internal I(10, ~city);

    LinkNodeLoad * linked I(11);
} BasicPlus;

#define GEN_LINK_REPORT(TYPE) \
void \
report_ ## TYPE (TYPE * node) { \
    if (node) { \
        do { \
            printf("%i -> ", node->value); \
            node = node->next; \
        } while (node); \
        printf("null\n"); \
    } \
}

GEN_LINK_REPORT(LinkNodeSave)
GEN_LINK_REPORT(LinkNodeLoad)

#include "city.c.intro"

int
main() {
    Basic obj_save;
    intro_set_defaults(&obj_save, ITYPE(Basic));
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
    obj_save.cool_number = 102928348;
    obj_save.character = "bingus";
    obj_save._internal = &obj_save.b;

    obj_save.text.buffer = "Long ago, 4 nations ruled over the earth in harmony, but everything changed when the fire nation attacked.";
    obj_save.text.bookmark = obj_save.text.buffer + 53;

    LinkNodeSave * node0 = calloc(1, sizeof(LinkNodeSave));
    node0->value = 0;

    LinkNodeSave * node1 = calloc(1, sizeof(LinkNodeSave));
    node1->value = 1;

    LinkNodeSave * node2 = calloc(1, sizeof(LinkNodeSave));
    node2->value = 2;

    LinkNodeSave * node3 = calloc(1, sizeof(LinkNodeSave));
    node3->value = 3;
    
    node3->next = node2;
    node2->next = node0;
    node0->next = node1;

    obj_save.linked = node3;

    int count_stuffs = 5;
    obj_save.count_stuffs = count_stuffs;
    obj_save.stuffs = calloc(count_stuffs, sizeof(obj_save.stuffs[0]));

    obj_save.stuffs[0].id = 15;
    obj_save.stuffs[0].name = "Burger King Foot Lettuce";

    obj_save.stuffs[1].id = 985;
    obj_save.stuffs[1].name = "Jeremy Elbertson";

    obj_save.stuffs[2].id = 12;
    obj_save.stuffs[2].name = "The Father";

    obj_save.stuffs[3].id = 9999;
    obj_save.stuffs[3].name = "Another one.";

    obj_save.stuffs[4].id = 2;
    obj_save.stuffs[4].name = NULL;

    printf("obj_save: Basic = ");
    intro_print(&obj_save, ITYPE(Basic), NULL);
    printf("\n");
    report_LinkNodeSave(obj_save.linked);
    printf("\n");

    bool create_success = intro_create_city_file("obj.cty", &obj_save, ITYPE(Basic));
    assert(create_success);

    BasicPlus obj_load;
    bool load_ok = intro_load_city_file(&obj_load, ITYPE(BasicPlus), "obj.cty");
    assert(load_ok);

    printf("obj_load: BasicPlus = ");
    intro_print(&obj_load, obj_load.type, NULL);
    printf("\n");
    report_LinkNodeLoad(obj_load.linked);

#define CHECK_EQUAL(member) \
    if (0!=memcmp(&obj_load.member, &obj_save.member, sizeof(obj_save.member))) { \
        fprintf(stderr, "NOT EQUAL (%s:%i): " #member, __FILE__, __LINE__); \
        exit(1); \
    }

#define CHECK_STR_EQUAL(member) \
    if (0 != strcmp(obj_load.member, obj_save.member)) { \
        fprintf(stderr, "NOT EQUAL (%s:%i): " #member, __FILE__, __LINE__); \
        exit(1); \
    }

    CHECK_EQUAL(a);
    assert(obj_save.b == obj_load.b2);
    CHECK_EQUAL(array);
    CHECK_EQUAL(wood_type);
    CHECK_EQUAL(stuff.a);
    CHECK_EQUAL(stuff.b);
    CHECK_STR_EQUAL(name);
    CHECK_EQUAL(count_numbers);
    assert(0==memcmp(obj_save.numbers, obj_load.numbers, obj_save.count_numbers * sizeof(obj_save.numbers[0])));
    CHECK_EQUAL(cool_number);
    CHECK_STR_EQUAL(character);
    CHECK_STR_EQUAL(long_member_name_that_would_take_up_a_great_deal_of_space_in_a_city_file);
    assert(obj_load._internal == NULL);

    LinkNodeSave * save_node = obj_save.linked;
    LinkNodeLoad * load_node = obj_load.linked;
    while (save_node && load_node) {
        assert(save_node->value == load_node->value);
        save_node = save_node->next;
        load_node = load_node->next;
    }

#define ABS(x) (((x) > 0)? (x) : -(x))

    CHECK_EQUAL(count_stuffs);
    for (int i=0; i < count_stuffs; i++) {
        assert(obj_load.stuffs[i].id == obj_save.stuffs[i].id);
        if (obj_save.stuffs[i].name == NULL) {
            assert(0==strcmp(obj_load.stuffs[i].name, "unnamed"));
        } else {
            assert(0==strcmp(obj_load.stuffs[i].name, obj_save.stuffs[i].name));
        }
        assert(obj_load.stuffs[i].hex == 0xFF56A420);
        assert(ABS(obj_load.stuffs[i].speed - 5.6) < 0.00001);
    }

    return 0;
}
