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
    enum {
        SEL_NONE = 0,
        SEL_STR,
        SEL_INT,
        SEL_FLOAT,
    } which I(0);

    I(1) union {
        size_t __data     I(~city, ~gui_show);
        const char * str  I(when <-which == SEL_STR);
        int int_value     I(when <-which == SEL_INT);
        float float_value I(when <-which == SEL_FLOAT);
    };
} Selection;

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

    Selection selections [4] I(12);
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

    Selection selections [4] I(12);
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
    intro_fallback(&obj_save, ITYPE(Basic));
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

    obj_save.selections[0] = (Selection){.which = SEL_INT,   .int_value = 45};
    obj_save.selections[1] = (Selection){.which = SEL_FLOAT, .float_value = -8.5};
    obj_save.selections[2] = (Selection){.which = SEL_STR,   .str = "Tom Meyers, Greatest ever comedian"};
    obj_save.selections[3] = (Selection){.which = SEL_FLOAT, .float_value = 9.75};

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
    intro_print(&obj_load, ITYPE(BasicPlus), NULL);
    printf("\n");
    report_LinkNodeLoad(obj_load.linked);

#define CHECK_EQUAL(MBR) \
    if (0!=memcmp(&obj_load.MBR, &obj_save.MBR, sizeof(obj_save.MBR))) { \
        fprintf(stderr, "NOT EQUAL (%s:%i): " #MBR "\n", __FILE__, __LINE__); \
        exit(1); \
    }

#define CHECK_STR_EQUAL(MBR) \
    if (!obj_load.MBR || 0 != strcmp(obj_load.MBR, obj_save.MBR)) { \
        fprintf(stderr, "NOT EQUAL (%s:%i): " #MBR "\n", __FILE__, __LINE__); \
        exit(1); \
    }

    const IntroMember * m = intro_member_by_name(ITYPE(BasicPlus), name);
    assert(m != NULL);
    assert(intro_has_attribute(m, cstring));

    CHECK_EQUAL(a);
    assert(obj_save.b == obj_load.b2);
    CHECK_EQUAL(array);
    CHECK_EQUAL(wood_type);
    CHECK_EQUAL(stuff.a);
    CHECK_EQUAL(stuff.b);
    CHECK_STR_EQUAL(name);
    CHECK_EQUAL(count_numbers);
    assert(obj_load.numbers && 0==memcmp(obj_save.numbers, obj_load.numbers, obj_save.count_numbers * sizeof(obj_save.numbers[0])));
    CHECK_EQUAL(cool_number);
    CHECK_STR_EQUAL(character);
    CHECK_STR_EQUAL(long_member_name_that_would_take_up_a_great_deal_of_space_in_a_city_file);
    assert(obj_load._internal == NULL);
    for (int i=0; i < LENGTH(obj_save.selections); i++) {
        CHECK_EQUAL(selections[i].which);
        switch(obj_load.selections[i].which) {
        case SEL_STR:
            CHECK_STR_EQUAL(selections[i].str);
            break;
        case SEL_INT:
            CHECK_EQUAL(selections[i].int_value);
            break;
        case SEL_FLOAT:
            CHECK_EQUAL(selections[i].float_value);
            break;
        default: break;
        }
    }

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
