#include "pager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void init_block(block_t *block){
    block->page_ptr = malloc(sizeof(unsigned char) * BLOCK_SIZE);
    block->next = NULL;
}

void fill_page(block_t *block, int blockID){
    int *vals = GET_HEADER(block, HEADERSIZE);  
    *vals = NUM_HEADERS;

    vals = GET_HEADER(block, BLOCKID);
    *vals = blockID;

    vals = GET_HEADER(block, CAPACITY);
    *vals = RECORD_AMOUNT;

    vals = GET_HEADER(block, AVAOFFSET);
    *vals = 0;
    
    int i;
    for(vals += 1, i = 0; i < RECORD_AMOUNT + 2; i++, vals += 1){  // ColumnID Recordcount freespace
        *vals = 0;
    }
}


int get_column_id(int ColumnID, block_t *block){
    int *col_id_ptr = GET_HEADER(block, COLUMNID);

    if(*col_id_ptr == ColumnID){
        return 1;
    }
    return 0;
}


int get_page(block_t *block, int record_count){
    int *cur_amount = GET_HEADER(block, RECORDCOUNT);

    if(*cur_amount != record_count){
        return 0;
    }
    return 1;
}


int insert_val(block_t *block, void *val, int ColumnID, int type, size_t size){

    int *val_to_insert = GET_HEADER(block, AVAOFFSET);             // AvaOffset
    int offset = *val_to_insert;
    if((offset + size) > (BLOCK_SIZE - (NUM_HEADERS * sizeof(int)))){
        return -1;
    }

    *val_to_insert += size;
    
    val_to_insert = GET_HEADER(block, CAPACITY);
    *val_to_insert = GET_MAXIMUM_ATTR_COUNT(size);

    unsigned char *insert_ptr = (unsigned char *)GET_HEADER(block, FREESPACE + offset); // Align to free space


    if (type == INT) {
        *(int *)insert_ptr = *(int *)val;  // Insert int
        //insert_ptr = *GET_HEADER(block, FREESPACE + offset);
        //printf("\n\nInserted %d at %d with blockID %d, colID %d\n\n", (int)insert_ptr, FREESPACE + offset, *GET_HEADER(block, BLOCKID), ColumnID);

    } else {
        memcpy(insert_ptr, (char *)val, size);  // Insert string or other types
        //insert_ptr = (char *)GET_HEADER(block, FREESPACE + offset);
        //printf("\n\nInserted %s at %d with blockID %d, colID %d\n\n", (char *)insert_ptr, FREESPACE + offset, *GET_HEADER(block, BLOCKID), ColumnID);
    }

  
    val_to_insert = GET_HEADER(block, COLUMNID);            // ColumnID 
    *val_to_insert = ColumnID;

    val_to_insert = GET_HEADER(block, RECORDCOUNT);            // RecordCount
    *val_to_insert += 1;


    return 1;
    //printf("BlockID: %d, Inserted: %d offset: %d, Base Address: %x\n", *(int *)(block->page_ptr + 4),
    //inserted_val, offset, base_address);
}


int insert_col_val(struct schema *sch, int ColumnID, void *val, int type, size_t size){

    int status;
    block_t *block;
    for(block = sch->first_block; block; block = block->next){
        // Check for block with corresponding columnID
        if(get_column_id(ColumnID, block)){
            // Find block with non-maxed row count
                status = insert_val(block, val, ColumnID, type, size);
                if(status == -1){
                    printf("Unable to input attribute value into current block\n"); 
                } else if(status == 1){
                    return 1;
                }               
        }   
    }
    
    for(block = sch->first_block; block; block = block->next){
        if(get_column_id(0, block)){
            insert_val(block, val, ColumnID, type, size);
            return 1;
        }
    }
    insert_block(sch);
    for(block = sch->first_block; block->next; block = block->next){
        ;
    }
    insert_val(block, val, ColumnID, type, size);

    return -1;
}

int compare_val(block_t *block, cmpfunc_t func, int val, int last_accessed_index){
    int *block_val = GET_HEADER(block, (FREESPACE + (4 * last_accessed_index)));

    int result = func(*block_val, val);
    return result;

}

void insert_ten(block_t *block, int ten){
    int *ptr = GET_HEADER(block, FREESPACE);

    *ptr = ten;

}
