#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "schema.h"
#include "storage.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s field:type:size [...]\n", argv[0]);
        return 1;
    }

    int field_amount = argc - 1;
    schema_t *sch = create_initial_schema(field_amount, MAX_RG_RECORD_AMOUNT);
    field_desc_t *head = NULL;
    field_desc_t *last = NULL;

    for (int i = 1; i < argc; i++) {
        char input_copy[256];
        strncpy(input_copy, argv[i], sizeof(input_copy) - 1);
        input_copy[sizeof(input_copy) - 1] = '\0';

        char *name_token = strtok(input_copy, ":");
        char *type_token = strtok(NULL, ":");
        char *size_token = strtok(NULL, ":");
        char *extra      = strtok(NULL, ":");

        if (!name_token || !type_token || !size_token) {
            printf("Error: '%s' must have format fieldname:type:size\n", argv[i]);
            return 1;
        }
        if (extra) {
            printf("Error: Too many colons in '%s'\n", argv[i]);
            return 1;
        }

        int name_len = strlen(name_token);
        if (name_len == 0) { printf("Error: Field name cannot be empty\n"); return 1; }
        if (name_len > 20) { printf("Error: Field name '%s' too long\n", name_token); return 1; }

        char *endptr;
        int type = strtol(type_token, &endptr, 10);
        if (*endptr != '\0' || (type != 0 && type != 1)) {
            printf("Error: Invalid type '%s' for '%s' (0=int, 1=string)\n", type_token, name_token);
            return 1;
        }

        int size = strtol(size_token, &endptr, 10);
        if (*endptr != '\0' || size <= 0) {
            printf("Error: Invalid size '%s' for '%s'\n", size_token, name_token);
            return 1;
        }
        if (size > 1024) printf("Warning: Large size %d for field '%s'\n", size, name_token);

        printf("Field %d: name='%s', type=%d (%s), size=%d\n",
               i, name_token, type, type == 0 ? "int" : "string", size);

        field_desc_t *fd = field_desc_init(size, name_token, type, i - 1);
        if (!head) head = fd;
        else last->next = fd;
        last = fd;
    }

    int fd = open_file("new_file1");
    if (!fd)                  { printf("FD init failed\n");           return 0; }
    if (!insert_magic(fd))    { printf("Failed writing header magic\n"); return 0; }
    init_row_group(fd, sch, head);
    insert_footer(fd, sch, head);
    if (!insert_magic(fd))    { printf("Failed writing footer magic\n"); return 0; }

    return 0;
}