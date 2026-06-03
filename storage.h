#ifndef STORAGE_H
#define STORAGE_H

#include "schema.h"
#include <stddef.h>

/*
 * --- Pager stub ---
 * When you implement the pager, replace BLOCK_SIZE / NUM_PAGES and the
 * page_t struct with your real definitions and move them into pager.h.
 * The functions below will then call pager_read()/pager_write() instead
 * of raw read()/write() — nothing above this layer needs to change.
 */
#define BLOCK_SIZE 256
#define NUM_PAGES  2

typedef struct page page_t;
struct page {
    char *content;
    int   page_nr;
    int   last_accessed;
    int   dirty;
    int   pinned;
};

extern page_t *pages[NUM_PAGES];

/* File lifecycle */
int  open_file(char *fname);
int  find_file(char *fname);
int  validate_file(int fd);
void lock_file(int fd);
void unlock_file(int fd);

/* Magic number */
int  insert_magic(int fd);

/* Row group / column init */
void init_column(int fd, schema_t *sch, int columnSize);
void init_row_group(int fd, schema_t *sch, field_desc_t *head);

/* Footer */
void insert_footer(int fd, schema_t *sch, field_desc_t *head);
void increment_record_amount(int fd, int record_amount);
int  calculate_column_offset(int totalRecords, int totalPreviousColumnSizes, int max_rg_size);

/* Record insertion */
record init_record(schema_t *sch, field_desc_t *head, char **col_vals);
record init_empty_record(schema_t *sch, field_desc_t *head);
void   insert_record(int fd, record rc, field_desc_t *head, schema_t *sch, int total_row_size);
int    insert_row(char *filename, char **col_vals);

/* Low-level helpers */
void  *read_chunk(int fd, size_t size);
void   print_hex_dump(char *buffer, size_t length);

#endif