#ifndef COLUMNSTORE_H
#define COLUMNSTORE_H

#include "pager.h"
#include "schema.h"


enum {
    EQUAL,
    NOT_EQUAL,
    LESS,
    LESS_OR_EQUAL,
    GREATER,
    GREATER_OR_EQUAL
};

// Initializes a record, allocating space for each corresponding field in the schema
record init_record(schema_t *sch);

// Assigns value at given address
void assign_int_field(void const *field_p, int val);

// Assigns string at given address
void assign_str_field(void *field_p, const char *str);

// Inserts the record's attributes to corresponding blocks in schema
void insert_record(schema_t *sch, record r);

// Frees memory allocated for schema
void release_record(schema_t *sch, record r);

// Generates a fully filled table
void generate_table(schema_t *sch);

// Prints attributes of record
void print_record(schema_t *sch, record rec);

// Prints all fields for given schema
void print_fields(schema_t *sch);

// Creates a projection on provided schema, constructing new schema based on provided attributes
void project(schema_t *sch, const char *attributes[], int attr_count);

// Inserts values at current position in each block into record
int get_record(schema_t *sch, record rc);

// Determines which operation to perform for selection
cmpfunc_t determine_op(char *op);

// Creates subschema with blocks of attributes that fulfill condition
schema_t *selection(schema_t *sch, const char *fields[], int count,char *conditional, int value, cmpfunc_t func);

// Fills sub record with values from source record based on fields in sub schema
void fill_sub_record(schema_t *src_sch, record src_rc, schema_t *dest_sch, record dest_rc);

// Prints relevant information about all blocks in a schema
void print_blocks(schema_t *sch);

// Does whatever is needed, used for testing
void test_function(schema_t *sch);

// Prints information about given block
void print_block(block_t *block, int type, size_t size);


// Different operators used for selection
static int int_equal(int x, int y);
static int int_not_equal(int x, int y);
static int int_less_than(int x, int y);
static int int_less_than_or_equal(int x, int y);
static int int_greater_than(int x, int y);
static int int_greater_than_or_equal(int x, int y);

#endif