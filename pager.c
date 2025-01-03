#include "pager.h"
#include <stdio.h>
#include <stdlib.h>

/*void init_pages(void){
    blocks = malloc(sizeof(block_t) * NUM_BLOCKS);
    for(int i = 0; i < NUM_BLOCKS; i++){
        blocks[i].page_ptr = malloc(sizeof(unsigned char) * BLOCK_SIZE); // * 4) * 6 + size * NUM_ATTR_VALUES);
        fill_page(&blocks[i], i);
    }
}*/

void init_block(block_t *block){
    block->page_ptr = malloc(sizeof(unsigned char) * BLOCK_SIZE);
}

void fill_page(block_t *block, int blockID){
    int *vals = GET_HEADER(block, HEADERSIZE);  
    *vals = NUM_HEADERS;

    vals = GET_HEADER(block, BLOCKID);
    *vals = blockID;

    vals = GET_HEADER(block, CAPACITY);
    *vals = MAX_ATTR_VALUES;

    vals = GET_HEADER(block, AVAOFFSET);
    *vals = 0;

    int i;
    for(vals += 1, i = 0; i < MAX_ATTR_VALUES + 2; i++, vals += 1){  // ColumnID Recordcount freespace
        *vals = 0;
    }
}


int get_column_id(int ColumnID, block_t *block){
    int *col_id_ptr = GET_HEADER(block, COLUMNID);

    //printf("got column id %d, looking for %d\n", *col_id_ptr, ColumnID);
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


void insert_val(block_t *block, int val, int ColumnID){
    int *val_to_insert = GET_HEADER(block, HEADERSIZE); // HeaderSize
    int base_address = val_to_insert; 
    
    val_to_insert = GET_HEADER(block, AVAOFFSET);             // AvaOffset
    int offset = *val_to_insert;
    *val_to_insert += 1;

    val_to_insert += offset + 3;   // FreeSpace
    *val_to_insert = val;
    int inserted_val = val;

  
    val_to_insert = GET_HEADER(block, COLUMNID);            // ColumnID 
    *val_to_insert = ColumnID;


    val_to_insert = GET_HEADER(block, RECORDCOUNT);            // RecordCount
    *val_to_insert += 1;

    //printf("BlockID: %d, Inserted: %d offset: %d, Base Address: %x\n", *(int *)(block->page_ptr + 4),
    //inserted_val, offset, base_address);
}


void insert_col_val(struct schema *sch, int ColumnID, int val){
    int j;
    for(int i = 0; i < NUM_BLOCKS; i++){
        // Check for block with corresponding columnID

        if(get_column_id(ColumnID, &sch->blocks[i])){
            
            // Find block with not-maxed row count
            if(!get_page(&sch->blocks[i], MAX_ATTR_VALUES)){
                insert_val(&sch->blocks[i], val, ColumnID);
                return;
            }
        }   
    }


    // if not, find first empty block and set columnID for empty block
    for(j = 0; !get_column_id(0, &sch->blocks[j]); j++){
        ;
    }
    insert_val(&sch->blocks[j], val, ColumnID);



}

void print_block(block_t *block){
    int *header_ptr = GET_HEADER(block, HEADERSIZE);
    printf("Header Amount: %d\n", *header_ptr);

   header_ptr = GET_HEADER(block, BLOCKID);
    printf("BlockID: %d\n", *header_ptr);
    header_ptr = GET_HEADER(block, CAPACITY);
    printf("Capacity: %d\n", *header_ptr);
    header_ptr = GET_HEADER(block, AVAOFFSET);
    printf("AvaOffset: %d\n", *header_ptr);
    header_ptr = GET_HEADER(block, COLUMNID);
    printf("ColumnID: %d\n", *header_ptr);
    header_ptr = GET_HEADER(block, RECORDCOUNT);
    printf("RecordCount: %d\n", *header_ptr);


    header_ptr += 1;
    for(int j = 0; j < MAX_ATTR_VALUES; j++, header_ptr += 1){
        printf("At %d lies value: %d\n", j, *header_ptr);

    }
    printf("\n\n");
}

int compare_val(block_t *block, cmpfunc_t func, int val, int index){
    int *header_ptr = GET_HEADER(block, FREESPACE + index * sizeof(int));
    printf("cmpval %d    %d     %d\n", index, val, *header_ptr);

    if(func(*header_ptr, val) == 1) return 1;

    return 0;

}

void insert_ten(block_t *block, int ten){
    int *ptr = GET_HEADER(block, FREESPACE);

    *ptr = ten;

}