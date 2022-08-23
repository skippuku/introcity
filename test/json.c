#include <stdio.h>
#include <stdlib.h>

#include "test.h"

int
main() {
    TestDefault obj;
    intro_default(&obj, ITYPE(TestDefault));

    char * buffer = malloc(1 << 16);
    intro_sprint_json_x(INTRO_CTX, buffer, &obj, ITYPE(TestDefault), NULL);

    puts(buffer);

    free(buffer);
    return 0;
}
