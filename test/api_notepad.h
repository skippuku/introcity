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

enum {
    IATTR_gui_show,
    IATTR_gui_color,
};

// impl notes

/*
    3 pools:
        global
        type
        declaration
*/

