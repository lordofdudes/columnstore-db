#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pager.h"
#include "schema.h"
#include "storage.h"
/*
 Handles INSERT query, verifying and passing input to pager
    filename: Name of file to insert into.
    col_vals: Values to insert.
 NOTE: Expects each column value to be strings, so an int attribute 
       like 465 would be the string "465" which later is converted back to an int
*/
int query_insert_row(char *filename, char **col_vals){
    char *FAIL = "INSERTION FAIL:";

    if(filename == NULL){
        printf("%s filename not provided\n", FAIL);
        return 0;
    }    

    if(col_vals == NULL){
        printf("%s no values provided", FAIL);
        return 0;
    }

    pager_insert_row(filename, col_vals);
    return 1;
}

int query_create_table(char *filename, char **field_args, int field_count) {
    schema_t *sch = create_initial_schema(field_count, MAX_RG_RECORD_AMOUNT);
    field_desc_t *head = NULL;
    field_desc_t *last = NULL;

    for (int i = 0; i < field_count; i++) {
        char input_copy[256];
        strncpy(input_copy, field_args[i], sizeof(input_copy) - 1);
        input_copy[sizeof(input_copy) - 1] = '\0';

        char *name_token = strtok(input_copy, ":");
        char *type_token = strtok(NULL, ":");
        char *size_token = strtok(NULL, ":");
        char *extra      = strtok(NULL, ":");

        if (!name_token || !type_token || !size_token || extra) {
            printf("Error: '%s' must have format fieldname:type:size\n", field_args[i]);
            return 0;
        }

        char *endptr;
        int type = strtol(type_token, &endptr, 10);
        if (*endptr != '\0' || (type != 0 && type != 1)) {
            printf("Error: Invalid type '%s' for '%s'\n", type_token, name_token);
            return 0;
        }

        int size = strtol(size_token, &endptr, 10);
        if (*endptr != '\0' || size <= 0) {
            printf("Error: Invalid size '%s' for '%s'\n", size_token, name_token);
            return 0;
        }

        field_desc_t *fd = field_desc_init(size, name_token, type, i);
        if (!head) head = fd;
        else last->next = fd;
        last = fd;
    }

    // Disallow overwriting existing files
    FILE *file = fopen(filename, "wx");
    if (!file) {
        perror("Error creating file");
        return 0;
    }
    fclose(file);

    int fd = open_file(filename);
    if (!fd)               { printf("FD init failed\n");            return 0; }
    if (!insert_magic(fd)) { printf("Failed writing header magic\n"); return 0; }
    init_row_group(fd, sch, head);
    insert_footer(fd, sch, head);
    if (!insert_magic(fd)) { printf("Failed writing footer magic\n"); return 0; }
    pager_init(fd);
    return 1;
}