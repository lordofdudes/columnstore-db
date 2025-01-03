#ifndef PAGER_H
#define PAGER_H

#include "schema.h"

#define NUM_HEADERS 6
#define MAX_ATTR_VALUES 10    // Each block can contain x rows
#define BLOCK_SIZE ((NUM_HEADERS * 4) + (MAX_ATTR_VALUES * 4))
#define NUM_BLOCKS 3
#define NUM_FIELDS 3

// Returns an int pointer to a given position in a block
#define GET_HEADER(block, header) ((int *)((block)->page_ptr + (header)))

// Header/metadata structure and general structure for block
//
// HeaderSize  [0 - 3]:   Total size of header (in bytes)
// BlockID     [4 - 7]:   Unique identifier for which block, ie BlockID = 0, first block, etc
// Capacity:   [8 - 11]:  Max amount of attribute values allowed 
// AvaOffset:  [12 - 15]: Next available position to insert numbers  
// ColumnID    [16 - 19]: Unique identifier for which column block belongs to, ie ColumnID = 1, "Age", etc
// RecordCount [20 - 23]: Current amount of records in block
// Free space: [24 - 43]: Free space for attribute values

struct block;
typedef struct block block_t;

struct block {
    unsigned char *page_ptr;
};


// Comparison function used for selection
typedef int (*cmpfunc_t)(int, int);

// Offsets (in bytes) for each of the metadata headers
enum {
    HEADERSIZE = 0 * 4,   // 0
    BLOCKID    = 1 * 4,   // 4
    CAPACITY   = 2 * 4,   // 8
    AVAOFFSET  = 3 * 4,   // 12
    COLUMNID   = 4 * 4,   // 16
    RECORDCOUNT= 5 * 4,   // 20
    FREESPACE  = 6 * 4    // 24
};


//void init_pages(void);

// Allocates space for block
void init_block(block_t *block);

// Initializes blocks, sets correct values for metadata/headers
void fill_page(block_t *block, int blockID);

// Returns 1 if block has record count equal provided record count, 0 otherwise
int get_page(block_t *block, int record_count);

// Returns 1 if block is corresponding blockID, 0 otherwise
int get_column_id(int ColumnID, block_t *block);

// Finds corresponding block of provided columnID
void insert_col_val(struct schema *sch, int ColumnID, int val);

// Actually does the performing of inserting value into block
void insert_val(block_t *block, int val, int ColumnID);

// Prints relevant information about blocks, like blockID, columnID, record count, etc
void print_block(block_t *block);

// Uses comparison function to determine if an attribute value fulfills condition
int compare_val(block_t *block, cmpfunc_t func, int val, int index);

// Auxilliary testing function to insert 10 into blockID 0, index 0
void insert_ten(block_t *block, int ten);

#endif