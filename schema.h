#ifndef SCHEMA_H
#define SCHEMA_H

/************************************************************
 * This file implements the storage layer, which is responsible for
 * managing the on-disk file format and providing an API for reading
 * and writing records.
 *
 * The file format is as follows:
 *
 *   [column 1 data] [column 2 data] ... [column n data]
 *   [footer region]
 *
 * The footer region contains the schema (field names, types, sizes,
 * and column offsets) and a magic number to validate the file type.
 *
 * When implementing the pager, the storage layer will go through
 * pager_read() / pager_write() instead of calling read()/write()
 * directly
 ************************************************************/

#define MAX_RG_RECORD_AMOUNT 16

typedef void **record;

typedef struct field_desc field_desc_t;
struct field_desc{
    int size;
    char name[20];
    int type;
    int ColumnID;
    struct field_desc *next;
};

typedef struct schema schema_t;
struct schema{
    int record_amount;
    int max_rg_record_amount;
    int field_amount;
};

schema_t     *create_initial_schema(int field_amount, int max_rg_record_amount);
field_desc_t *field_desc_init(int size, char *name, int type, int columnID);
int           insert_field(field_desc_t *head, field_desc_t *insert);
void          print_schema(schema_t *sch);
void          print_field_descriptors(field_desc_t *head);
void          reconstruct_schema(int fd, schema_t *sch, field_desc_t **head);
void          print_record(record rc);

typedef int (*cmpfunc_t)(int, int);

char      **filter(int fd, schema_t *sch, field_desc_t *head, char *filtered_col, int amount, int *ptr2, char *op);
char      **project(schema_t *sch, field_desc_t *head, char **vals, int num_vals, char **cols, int num_cols);
char      **parse_query(char **cols, int num_cols, char *filtered_col, int amount, int *res2, char *op);
char      **return_all(int *outgoing_row_amount);

int         parse_ints(void *chunk, int num_vals, int val, cmpfunc_t cmp_op);
cmpfunc_t   determine_op(char *op);

void free_schema(schema_t *sch, field_desc_t *head);

#endif