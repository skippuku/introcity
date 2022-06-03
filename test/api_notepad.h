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
    value(FuncOnNode) pre_draw,
))

I(attribute i_ (
    alias: const(char * []),
))

typedef struct {char * name; int value;} CyContent;

I(attribute cy_ (
    favorite: flag,
    priority: int,
    widget:   const(FuncOnNode),
    hard_mode_default: const(@inherit),
    options: const(CyContent []),
))

struct Settings {
    int count I(gui_: ~show);
    Color3f I(gui_: edit_color, color 0x11aa66);
};

// impl notes

enum {
    IATTR_gui_show,
    IATTR_gui_color,
};

//
// 3 pools:
//     global
//     type
//     declaration
//
