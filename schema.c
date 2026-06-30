#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "schema.h"
#include "storage.h"


schema_t *create_initial_schema(int field_amount, int max_rg_record_amount) {
    schema_t *sch = malloc(sizeof(schema_t));
    sch->field_amount = field_amount;
    sch->max_rg_record_amount = max_rg_record_amount;
    sch->record_amount = 0;
    return sch;
}

field_desc_t *field_desc_init(int size, char *name, int type, int columnID) {
    field_desc_t *fd = malloc(sizeof(field_desc_t));
    fd->size = size;
    memset(fd->name, 0, 20);
    strncpy(fd->name, name, 19);
    fd->name[19] = '\0';
    fd->type = type;
    fd->ColumnID = columnID;
    fd->next = NULL;
    return fd;
}

int insert_field(field_desc_t *head, field_desc_t *insert) {
    if (head == NULL) return 1;
    int i = 0;
    field_desc_t *tmp = head;
    while (tmp->next) {
        if (strncmp(tmp->name, insert->name, 20) == 0) {
            printf("%s already exists at position %d\n", tmp->name, i);
            return 0;
        }
        tmp = tmp->next;
        i++;
    }
    if (strncmp(tmp->name, insert->name, 20) == 0) {
        printf("%s already exists at position %d\n", tmp->name, i);
        return 0;
    }
    tmp->next = insert;
    return 1;
}

void print_schema(schema_t *sch) {
    printf("SCHEMA: record amount %d, max record amount %d, field amount %d\n",
           sch->record_amount, sch->max_rg_record_amount, sch->field_amount);
}

void print_field_descriptors(field_desc_t *head) {
    int i = 0;
    for (field_desc_t *cur = head; cur; cur = cur->next, i++) {
        printf("FIELD %d: name %s, size %d, type %d, ColumnID %d\n",
               i, cur->name, cur->size, cur->type, cur->ColumnID);
    }
}

/*
 * Reads the footer region (caller must have already seeked to the top of it)
 * and populates sch + builds the field_desc linked list.
 * NOTE: Once the pager is implemented, this will go through pager_read() instead
 * of calling read() directly, but the logic should be the same.
 */
void reconstruct_schema(int fd, schema_t *sch, field_desc_t **head) {
    read(fd, sch, sizeof(schema_t));
    print_schema(sch);

    field_desc_t *prev = NULL;
    for(int i = 0; i < sch->field_amount; i++){
        field_desc_t *field = malloc(sizeof(field_desc_t));

        read(fd, field, sizeof(field_desc_t) - 0x8);
        field->next = NULL;
        if(prev == NULL) *head = field;
        else prev->next = field;
        prev = field;
    }
}

/* ------------------------------------------------------------------ */
/* Comparison operators                                                 */
/* ------------------------------------------------------------------ */

int int_equal(int x, int y)              { return x == y; }
int int_not_equal(int x, int y)          { return x != y; }
int int_less_than(int x, int y)          { return x < y; }
int int_less_than_equal(int x, int y)    { return x <= y; }
int int_greater_than(int x, int y)       { return x > y; }
int int_greater_than_equal(int x, int y) { return x >= y; }

cmpfunc_t determine_op(char *op) {
    if (strcmp(op, "=")  == 0) return int_equal;
    if (strcmp(op, "!=") == 0) return int_not_equal;
    if (strcmp(op, "<")  == 0) return int_less_than;
    if (strcmp(op, "<=") == 0) return int_less_than_equal;
    if (strcmp(op, ">")  == 0) return int_greater_than;
    if (strcmp(op, ">=") == 0) return int_greater_than_equal;
    return NULL;
}

int parse_ints(void *chunk, int num_vals, int val, cmpfunc_t cmp_op) {
    int *ptr = (int *)chunk;
    int count = 0;
    for(int i = 0; i < num_vals; i++)
        if(cmp_op(ptr[i], val)) count++;
    return count;
}

/* ------------------------------------------------------------------ */
/* Project — pick named columns out of a flat row array               */
/* ------------------------------------------------------------------ */

char **project(schema_t *sch, field_desc_t *head,
               char **vals, int num_vals, char **cols, int num_cols) {
    int num_rows = num_vals / sch->field_amount;
    char **arr = malloc(sizeof(char *) * num_cols * num_rows);

    for (int row = 0; row < num_rows; row++) {
        for (int c = 0; c < num_cols; c++) {
            int col_id = -1;
            for (field_desc_t *fd = head; fd; fd = fd->next) {
                if (strcmp(fd->name, cols[c]) == 0) { col_id = fd->ColumnID; break; }
            }
            if (col_id == -1) { printf("Column %s not found\n", cols[c]); continue; }
            arr[row * num_cols + c] = strdup(vals[row * sch->field_amount + col_id]);
            printf("got %s\n", arr[row * num_cols + c]);
        }
    }
    printf("finishing\n");
    return arr;
}

/* ------------------------------------------------------------------ */
/* Filter — return all rows where filtered_col op amount is true      */
/* ------------------------------------------------------------------ */

char **filter(int fd, schema_t *sch, field_desc_t *head,
              char *filtered_col, int amount, int *ptr2, char *op) {
    cmpfunc_t cmp_op = determine_op(op);
    if (!cmp_op) { printf("Invalid operator %s\n", op); return NULL; }

    int *col_offsets = malloc(sizeof(int) * sch->field_amount);
    int col_id = -1, idx = 0, final_offset = 0, cur_size = 0;

    for (field_desc_t *cur = head; cur; cur = cur->next, idx++) {
        int offset;
        read(fd, &offset, sizeof(int));
        col_offsets[idx] = offset;
        if (strcmp(cur->name, filtered_col) == 0) {
            final_offset = offset;
            cur_size = cur->size;
            col_id = cur->ColumnID;
        }
    }

    if (col_id == -1) {
        printf("Column %s does not exist in table\n", filtered_col);
        free(col_offsets);
        return NULL;
    }

    lseek(fd, final_offset, SEEK_SET);
    int remainder     = sch->record_amount % sch->max_rg_record_amount;
    int group_records = (remainder == 0) ? sch->max_rg_record_amount : remainder;
    void *chunk       = read_chunk(fd, cur_size * group_records);
    int   match_count = parse_ints(chunk, group_records, amount, cmp_op);

    char **arr    = malloc(sizeof(char *) * sch->field_amount * match_count);
    int out_index = 0;
    int *iptr     = (int *)chunk;

    for (int row = 0; row < group_records; row++) {
        if (!cmp_op(iptr[row], amount)) continue;

        field_desc_t *col = head;
        for (int c = 0; c < sch->field_amount; c++, col = col->next) {
            lseek(fd, col_offsets[c] + row * col->size, SEEK_SET);
            void *buf = malloc(col->size);
            read(fd, buf, col->size);

            char tmp[71];
            if (col->type == 0) {
                int v; memcpy(&v, buf, sizeof(int));
                snprintf(tmp, sizeof(tmp), "%d", v);
            } else {
                snprintf(tmp, sizeof(tmp), "%s", (char *)buf);
            }
            arr[out_index++] = strdup(tmp);
            free(buf);
            *ptr2 += 1;
        }
    }

    free(chunk);
    free(col_offsets);
    return arr;
}

/* ------------------------------------------------------------------ */
/* parse_query — high-level SELECT col,... WHERE col op val           */
/* ------------------------------------------------------------------ */

char **parse_query(char **cols, int num_cols,
                   char *filtered_col, int amount, int *res2, char *op) {
    schema_t sch;
    field_desc_t *head = NULL;
    int num = 0;

    int fd = find_file("new_file1");
    if (!validate_file(fd)) { printf("Wrong file type\n"); return NULL; }

    int footer_size;
    lseek(fd, -0x8, SEEK_END);
    read(fd, &footer_size, sizeof(int));
    lseek(fd, -0x8 - footer_size, SEEK_END);
    reconstruct_schema(fd, &sch, &head);

    char **vals = filter(fd, &sch, head, filtered_col, amount, &num, op);
    char **res  = project(&sch, head, vals, num, cols, num_cols);
    *res2 = num_cols * (num / sch.field_amount);
    return res;
}

/* ------------------------------------------------------------------ */
/* return_all — dump every row                                         */
/* ------------------------------------------------------------------ */

char **return_all(int *outgoing_row_amount) {
    int fd = find_file("new_file1");
    if (!fd) { printf("failed to find file\n"); return NULL; }
    if (!validate_file(fd)) { printf("Failed to validate file in return_all\n"); return NULL; }

    lseek(fd, -0x8, SEEK_END);
    int footer_size;
    read(fd, &footer_size, sizeof(int));
    printf("got footer size %d\n", footer_size);

    schema_t sch;
    field_desc_t *head = NULL;
    lseek(fd, -0x8 - footer_size, SEEK_END);
    reconstruct_schema(fd, &sch, &head);

    if (sch.record_amount == 0) return NULL;
    *outgoing_row_amount = sch.record_amount;

    int offset;
    read(fd, &offset, sizeof(int));
    lseek(fd, offset, SEEK_SET);

    int total_sizes = 0;
    for (field_desc_t *f = head; f; f = f->next) total_sizes += f->size + 1;

    char **rows = malloc(sizeof(char *) * sch.record_amount);
    for (int i = 0; i < sch.record_amount; i++)
        rows[i] = calloc(1, total_sizes + 1);

    for (field_desc_t *fid = head; fid; fid = fid->next) {
        void *chunk = read_chunk(fd, sch.max_rg_record_amount * fid->size);
        for (int i = 0; i < sch.record_amount; i++) {
            char temp[71] = {0};
            if (fid->type == 0) {
                int val; memcpy(&val, (char *)chunk + i * fid->size, sizeof(int));
                snprintf(temp, sizeof(temp), "%d", val);
            } else {
                strncpy(temp, (char *)chunk + i * fid->size, fid->size);
                temp[fid->size] = '\0';
            }
            strcat(rows[i], temp);
            strcat(rows[i], ",");
        }
        free(chunk);
    }
    return rows;
}


void free_schema(schema_t *sch, field_desc_t *head) {
    free(sch);
    field_desc_t *cur = head;
    while (cur) {
        field_desc_t *next = cur->next;
        free(cur);
        cur = next;
    }
}