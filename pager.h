#ifndef PAGER_H
#define PAGER_H

#include "schema.h"

#define NUM_HEADERS 6 // 40(6*4) / 12
#define RECORD_AMOUNT 10    // Each block can contain x rows
#define BLOCK_SIZE ((NUM_HEADERS * sizeof(int)) + (RECORD_AMOUNT * 4))
//#define BLOCK_SIZE 512
#define NUM_BLOCKS 3
#define NUM_FIELDS 3

// Returns an int pointer to a given position in a block
#define GET_HEADER(block, header) ((int *)((block)->page_ptr + (header)))

// Returns maximum amount of attributes allowed in block given attribute size
#define GET_MAXIMUM_ATTR_COUNT(size) ((BLOCK_SIZE - (NUM_HEADERS * sizeof(int))) / size)

// Header/metadata structure and general structure for block
//
// HeaderSize  [0 - 3]:   Amount of headers
// BlockID     [4 - 7]:   Unique identifier for which block, ie BlockID = 0, first block, etc
// Capacity:   [8 - 11]:  Max amount of attribute values allowed 
// AvaOffset:  [12 - 15]: Next available position to insert numbers  
// ColumnID    [16 - 19]: Unique identifier for which column block belongs to, ie ColumnID = 1, "Age", etc
// RecordCount [20 - 23]: Current amount of records in block
// Free space: [24 - 43]: Free space for attribute values

struct block;
typedef struct block block_t;

struct block {
    struct block *next;
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

// Returns 1 if block is corresponding columnID, 0 otherwise
int get_column_id(int ColumnID, block_t *block);

// Finds corresponding block of provided columnID
int insert_col_val(struct schema *sch, int ColumnID, void *val, int type, size_t size);

// Actually does the performing of inserting value into block, returns -1 if block is filled
int insert_val(block_t *block, void *val, int ColumnID, int type, size_t size);

// Uses comparison function to determine if an attribute value fulfills condition
// Assumes only int type comparisons, so no WHERE name = "Jon Smith", 
// WHERE age = 25
int compare_val(block_t *block, cmpfunc_t func, int val, int last_accessed_index);

// Auxilliary testing function to insert 10 into blockID 0, index 0
void insert_ten(block_t *block, int ten);


#endif