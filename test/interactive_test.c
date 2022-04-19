#include <stdint.h>
#include <string.h>

#include "../lib/intro.h"
#include "../util.c"

typedef int32_t s32;
typedef struct {
    s32 demo I(default 22);
    char * message I(default "empty message");
} SaveData;

#ifndef __INTRO__
# include "interactive_test.c.intro"
#endif

int
main() {
    SaveData save;

    size_t last_file_data_length;
    void * last_file_data = read_entire_file("save.cty", &last_file_data_length);
    if (last_file_data) {
        intro_load_city(&save, ITYPE(SaveData), last_file_data, last_file_data_length);
    } else {
        intro_set_defaults(&save, ITYPE(SaveData));
    }

    printf("Save file contents:\n");
    intro_print(&save, ITYPE(SaveData), NULL);

    char new_message [128];
    printf("\nEnter a new message: ");
    fgets(new_message, sizeof(new_message), stdin);
    char * nl = strchr(new_message, '\n');
    if (nl) *nl = '\0';

    save.message = new_message;
    save.demo++;

    size_t new_file_data_length;
    void * new_file_data = intro_create_city(&save, ITYPE(SaveData), &new_file_data_length);
    dump_to_file("save.cty", new_file_data, new_file_data_length);

    free(new_file_data);
    free(last_file_data);

    return 0;
}
