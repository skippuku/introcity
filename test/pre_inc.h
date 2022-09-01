#define SOMETHING floof
#define FLOAT SOMETHING

typedef struct /* comment */ {
    int thing;
    FLOAT power; // intentional error
} cheese;
