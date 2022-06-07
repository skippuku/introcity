// value with no parenthesis is same as value(@inherit)

typedef union {
    struct{ float x,y; };
    float e [2] I(~gui_show);
} vec2 I(gui_vector, gui_color {0.0, 1.0, 1.0}, special);

// same as
typedef union {
    struct{ float x,y; };
    float e [2];
} vec2;

I(apply_to vec2 (
    gui_: vector, color {0.0, 1.0, 1.0},
    @global: special,
))

I(apply_to vec2.e (~gui_show))

// this is so you can apply attributes to other people's types without messying up their code

// ideas

I(apply_to vec2.e (~gui_show))

I(apply_to VertexGroup.* (~gui_edit))
I(apply_to VertexGroup.vertices (gui_edit))

I(apply_to VertexGroup.(* ~vertices) (~gui_edit))

I(apply_to DynArray_*.(capacity, length) Map_*.(* ~data) (~gui_edit))

typedef void (*FuncOnNode)(void *, const IntroType * type);

I(attribute gui_ (
    pre_draw: value(FuncOnNode),
))

I(attribute i_ (
    alias: value(char * []),
))

typedef struct {char * name; int value;} CyContent;

I(attribute cy_ (
    favorite: flag,
    priority: int,
    widget:   value(FuncOnNode),
    hard_mode_default: value(@inherit),
    options: value(CyContent []),
))

struct Settings {
    int count I(gui_: ~show);
    Color3f I(gui_: edit_color, color 0x11aa66);
};

// idea: when a value is a flexible array type ex. value(char * []) allow multiple instances of the attribute. additional instances add to the array

// idea: allow specifying a function to do compile-time validation checks like builtins do
I(attribute (
    generation: int @check(check_generation_attribute),
))

// members before pointer

typedef struct {
    size_t length;
    size_t capacity;
} stbds_array_header;

struct {
    //int * ids I(pre_header stbds_array_header);

    int * ids I(length length_ids);
    I(ghost length_ids ((stbds_array_header *) .ids - 1)->length);

    int version;
    union {
        int i I(when ..version == VER_INT);
        float f I(when ..version == VER_FLOAT);
    } u;

    int enum_like I(as SomeEnum);

    int t_width;
    int t_height;
    Tile * tiles I(length .t_width * .t_height);
};
