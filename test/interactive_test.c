#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <intro.h>

typedef int32_t s32;
typedef struct {
    s32 demo I(fallback 22);
    char * message I(fallback "empty message");
} SaveData;

#include "interactive_test.c.intro"

void
get_input_line(char * o_buf, size_t buf_size) {
    fgets(o_buf, buf_size, stdin);
    char * nl = strchr(o_buf, '\n');
    if (nl) *nl = '\0';
}

int
main() {
    SaveData save;

    bool from_file = intro_load_city_file(&save, ITYPE(SaveData), "save.cty");
    if (!from_file) {
        intro_fallback(&save, ITYPE(SaveData));
    }

    printf("Save file contents:\n");
    intro_print(&save, ITYPE(SaveData), NULL);
    if (from_file) free(save.message);

    char new_message [128];
    printf("\nEnter a new message: ");
    get_input_line(new_message, sizeof new_message);

    save.message = new_message;
    save.demo++;

    intro_create_city_file("save.cty", &save, ITYPE(SaveData));

    return 0;
}
