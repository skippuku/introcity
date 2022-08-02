#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../lib/intro.h"

typedef int32_t s32;
typedef struct {
    s32 demo I(default 22);
    char * message I(default "empty message");
} SaveData;

#include "interactive_test.c.intro"

int
main() {
    SaveData save;

    bool from_file = intro_load_city_file(&save, ITYPE(SaveData), "save.cty");
    if (!from_file) {
        intro_set_defaults(&save, ITYPE(SaveData));
    }

    printf("Save file contents:\n");
    intro_print(&save, ITYPE(SaveData), NULL);
    if (from_file) free(save.message);

    char new_message [128];
    printf("\nEnter a new message: ");
    fgets(new_message, sizeof(new_message), stdin);
    char * nl = strchr(new_message, '\n');
    if (nl) *nl = '\0';

    save.message = new_message;
    save.demo++;

    intro_create_city_file("save.cty", &save, ITYPE(SaveData));

    return 0;
}
