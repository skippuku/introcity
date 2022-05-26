I(attribute (
    @namespace gui_,
    float scale,
    value(@inherit) min,
    value(@inherit) max,
    value(Color3f) color @global({1.0, 1.0, 1.0}),
    flag  log,
    flag  edit_color,
    flag  vector,
    flag  show @global,
    flag  edit @global,
))

I(attribute (
    @namespace intro_,
    int     id,
    value(@inherit) default,
    member  length,
    flag    type,
    string  note,
    string  alias,
))

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

I(on_type vec2 (
    gui_: vector, color {0.0, 1.0, 1.0},
    @global: special,
))

I(on_type vec2.e (~gui_show))

// this is so you can apply attributes to other people's types without messying up their code

if (intro_has_attribute(&member, gui_show)) {
}

#define intro_has_attribute(p_member, attr) intro_has_attribute_x(INTRO_CTX, p_member, #attr)

INTRO_INLINE bool
intro_get_variant(void * dest, const IntroType * type, const IntroVariant * var) {
    if (type != var->type) {
        return false;
    } else {
        if (var->_data) {
            memcpy(dest, var->_data, intro_size(type));
        }
        return true;
    }
}

// impl notes

/*
    use memoization for querying attributes
    if any of the pools are modified for some reason, all attribute memoization must be reset for the context

    3 pools:
        global
        type
        declaration
*/
