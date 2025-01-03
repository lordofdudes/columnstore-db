#ifndef SCHEMA_H
#define SCHEMA_H

#include <stdlib.h>
#include "pager.h"
// Field descriptors, information about certain fields in records
//
// Name: Official name for field
// Size: Size of field (in bytes)
// ColumnID: Identifier that uniquely identifies field descriptor to corresponding blocks
// Next: Next field descriptor in list

struct field_desc;
typedef struct field_desc field_desc_t;

struct schema;
typedef struct schema schema_t;

struct field_desc{
    char *name;
    int ColumnID;
    int size;
    struct field_desc *next;
};

// Schema, structure that contains all records and fields descriptors
//
// Sch-name: Schema name
// First: First field descriptor
// Field_amount: Amount of fields in schema
// Last_accessed: Offset in page that was last accessed
// Record_amount: Current amount of records in schema

struct schema{
    char *sch_name;
    field_desc_t *first;
    int field_amount;
    int last_accessed;
    int record_amount;
    struct block *blocks;
};


// Contains all the data in a row
typedef void ** record;

// Initializes a schema, allocating blocks and setting metadata
schema_t *schema_init(char *name, int num_blocks);

// Adds a field descriptor to schema, adding a new field to schema
int add_field(schema_t *sch, field_desc_t *fd);

// Constructs new sub schema containing all or some of fields in original schema
schema_t *make_sub_schema(schema_t *sch, int num_fields, const char *fields[]);

// Creates a duplicate of given field descriptor f
field_desc_t *dup_field(field_desc_t *f, size_t size);

// Returns field descriptor if schema contains it already
field_desc_t *get_field(schema_t *sch, const char *name);

// Initializes new field descriptor with information like name, size, etc
field_desc_t *fd_init(char *name, size_t size);


#endif