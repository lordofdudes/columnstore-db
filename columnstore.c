#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "columnstore.h"
#include "pager.h"
#include "schema.h"

// ============================== Record functions ================================

record init_record(schema_t *sch){
    record rc = malloc(sizeof(void *) * sch->field_amount);
    int i = 0;
    for(field_desc_t *fd = sch->first; fd; fd = fd->next, i++){
        rc[i] = malloc(fd->size);
    }
    
    return rc;
}

void release_record(schema_t *sch, record r) {
    for(int i = 0; i < sch->field_amount; i++){
        free(r[i]);
    }

  free(r);
  r = 0;
}

void assign_int_field(void const *field_p, int val) {
  *(int *)field_p = val;
}

void assign_str_field(void *field_p, char const *str){
    strcpy((char *)field_p, str);
}

void insert_record(schema_t *sch, record r){
    int i = 0;
    int status = 0;
    for(field_desc_t *fd = sch->first; fd; fd = fd->next, i++){

        if(fd->type == INT) insert_col_val(sch, i + 1, r[i], INT, fd->size);
        else insert_col_val(sch, i + 1, r[i], STR, fd->size);
    }
 
    sch->record_amount++;
}


int get_record(schema_t *sch, record rc){
    if(sch->last_accessed == sch->record_amount) return 0;
    
    int i = 0;
    int j = 0;
    int current_record_amount = 0;
    int found = 0;
    field_desc_t *fd;
    block_t *block;
    for(fd = sch->first; fd; fd = fd->next, i++){ 
        for(j = 0, block = sch->first_block; block; block = block->next){
            if(get_column_id(fd->ColumnID, block)){
                j++;
                current_record_amount = *GET_HEADER(block, RECORDCOUNT);
                if(sch->last_accessed < (*GET_HEADER(block, CAPACITY) * j)){
                    if(fd->type == INT){
                            
                        int *attr_ptr = GET_HEADER(block, FREESPACE + (sizeof(int) * 
                                                           (sch->last_accessed % current_record_amount)));
                        *(int *)rc[i] = *attr_ptr;
                    }
                    else {  
                        char *str_ptr = (char *)GET_HEADER(block, FREESPACE + (fd->size * 
                                                           (sch->last_accessed % current_record_amount)));
                        strcpy((char*)rc[i], str_ptr);


                    }
                    found = 1;
                } else {
                    continue;
                }
            }

            if(found){
                found = 0;
                break;
            }
        }
    }

    sch->last_accessed++;
    return 1;
}

void fill_sub_record(schema_t *src_sch, record src_rc, schema_t *dest_sch, record dest_rc){
    int i = 0;
    int j = 0;
    for(field_desc_t *fd = src_sch->first; fd; fd = fd->next, j++){
        i = 0;
        for(field_desc_t *fd2 = dest_sch->first; fd2; fd2 = fd2->next, i++){

            if(strcmp(fd->name, fd2->name) == 0){
                if(fd->type == INT){
                    assign_int_field(dest_rc[i], *(int *)src_rc[j]);
                } else {
                    assign_str_field(dest_rc[i], (char *)src_rc[j]);
                }   
            }
        }
    }
}


// ============================ Auxiliary functions ==================================

void print_fields(schema_t *sch) {
    printf("|");
    for (field_desc_t *fd = sch->first; fd; fd = fd->next) {
        printf(" %-12s |", fd->name); // Print field name with padding for alignment
    }
    printf("\n");
}

void print_block(block_t *block, int type, size_t size){
    int *header_ptr = GET_HEADER(block, HEADERSIZE);
    printf("Header Amount: %d\n", *header_ptr);

    header_ptr = GET_HEADER(block, BLOCKID);
    printf("BlockID: %d\n", *header_ptr);
    header_ptr = GET_HEADER(block, CAPACITY);
    printf("Capacity: %d\n", *header_ptr);
    header_ptr = GET_HEADER(block, AVAOFFSET);
    int offset = *header_ptr;
    printf("AvaOffset: %d\n", *header_ptr);
    header_ptr = GET_HEADER(block, COLUMNID);
    printf("ColumnID: %d\n", *header_ptr);
    header_ptr = GET_HEADER(block, RECORDCOUNT);
    printf("RecordCount: %d\n", *header_ptr);

    // Calculate the start of data section
    unsigned char *data_ptr = (unsigned char *)GET_HEADER(block, FREESPACE);  // Align to data start
    printf("\n--- Data Section ---\n");

    for (int j = 0; j < *GET_HEADER(block, RECORDCOUNT); j++) {
        if (type == INT) {
            int val = *(int *)(data_ptr);  // Read integer
            printf("At %d lies value: %d\n", j, val);
        } else {
            char str[size + 1];  // Buffer for string (ensure null termination)
            memcpy(str, data_ptr, size);
            str[size] = '\0';  // Null-terminate the string
            printf("At %d lies string: %s\n", j, str);
        }
        data_ptr += size;  // Move by the field size for the next record
    }
    printf("\n\n");
}


void test_function(schema_t *sch){
    for(block_t *block = sch->first_block; block; block = block->next){
        printf("Got block %x with blockID %d, columnID %d\n", block, *GET_HEADER(block, BLOCKID), *GET_HEADER(block, COLUMNID));
    }
}

void generate_table(schema_t *sch){
    record rec = init_record(sch); 
    int i;
    char test_str[12];
    print_fields(sch);
    for(int j = 0; j < RECORD_AMOUNT; j++){
        i = 0;
        for(field_desc_t *fd = sch->first; fd; fd = fd->next, i++){
            if(fd->type == INT){ // Int type field
                assign_int_field(rec[i], rand() % 10000);
            }else{
                sprintf(test_str, "%s_%d", sch->sch_name, j);
                assign_str_field(rec[i], test_str);
            }
        }
        print_record(sch, rec);
        insert_record(sch, rec);
        

    }
}

void print_record(schema_t *sch, record rec) {
    int i = 0;
    printf("|");
    for (field_desc_t *fd = sch->first; fd; fd = fd->next, i++) {
        if (fd->type == INT) {
            printf(" %-12d |", *(int *)rec[i]); // Print integer value
        } else { 
            printf(" %-12s |", (char *)rec[i]); // Print string value
        }
    }
    printf("\n");
}

void print_blocks(schema_t *sch){
    for(field_desc_t *fd = sch->first; fd; fd = fd->next){
        for(block_t *block = sch->first_block; block; block = block->next){
            if(get_column_id(fd->ColumnID, block)){
                print_block(block, fd->type, fd->size);
            }
        }
    }
}

static int int_equal(int x, int y) { return x == y; }
static int int_not_equal(int x, int y) { return x != y; }
static int int_less_than(int x, int y) { return x < y; }
static int int_less_than_or_equal(int x, int y) { return x <= y; }
static int int_greater_than(int x, int y) { return x > y; }
static int int_greater_than_or_equal(int x, int y) { return x >= y; }

cmpfunc_t determine_op(char *op){
    if(!op){
        printf("Operator provided is NULL\n");
        return NULL;
    }

    cmpfunc_t func = NULL;

    if(strcmp(op, "=") == 0){
        return int_equal;
    }    
    else if(strcmp(op, "!=") == 0){ 
        return int_not_equal;
    }
    else if(strcmp(op, ">") == 0){
        return int_greater_than;
    }
    else if(strcmp(op, ">=") == 0){ 
        return int_greater_than_or_equal;
    }
    else if(strcmp(op, "<") == 0){ 
        return int_less_than;
    }
    else if(strcmp(op, "<=") == 0){
        return int_less_than_or_equal;
    }
    else if(strcmp(op, "==") == 0){
        return 1;
    }
    return func;
}

// ============================== Query operations ==================================

void project(schema_t *sch, const char *attributes[], int attr_count){
    if(attr_count == sch->field_amount){
       record rc1 = init_record(sch);
        print_fields(sch);
        while(get_record(sch, rc1)){
            print_record(sch, rc1);
        }
        release_record(sch, rc1);
        return;
    }

    schema_t *projection_sch = make_sub_schema(sch, attr_count, attributes);
    record rc2 = init_record(sch);
    record sub_record = init_record(projection_sch);
    while(get_record(sch, rc2)){
        fill_sub_record(sch, rc2, projection_sch, sub_record);
        insert_record(projection_sch, sub_record);
    }
    print_fields(projection_sch);
    while(get_record(projection_sch, sub_record)){
        print_record(projection_sch, sub_record);
    }    
    release_record(projection_sch, sub_record);

}

schema_t *selection(schema_t *sch, const char *fields[], int count,
                    char *conditional, int value, cmpfunc_t func){
    int i = 0;
    schema_t *selected_schema = make_sub_schema(sch, count, fields);
    record src_rc = init_record(sch);
    record dest_rc = init_record(selected_schema);

    // Go through each field descriptor in the source schema
    for(field_desc_t *fd = sch->first; fd; fd = fd->next, i++){

        // If selected attribute is same as field descriptor
        if(strcmp(fd->name, conditional) == 0){

            // Go through each block in source schema
            for(block_t *block = sch->first_block; block; block = block->next){
                // Check if corresponding columnID for block and field descriptor
                if(get_column_id(fd->ColumnID, block)){

                    // Go through all stored records in source schema
                    for(int j = 0; j < *GET_HEADER(block, RECORDCOUNT); j++){ // HERE
                        get_record(sch, src_rc);
                        if(compare_val(block, func, value, j)){
                            fill_sub_record(sch, src_rc, selected_schema, dest_rc);
                            insert_record(selected_schema, dest_rc);

                        }

                    }
                }
            }

        }
    }
    release_record(sch, src_rc);
    release_record(selected_schema, dest_rc);
    return selected_schema;
}

// ============================== Main loop/Interpreter ==========================================

// SELECT * FROM t1 WHERE age = 10

int main() {
    srand(time(NULL));
    //init_pages();
    int result;

    schema_t *sch = schema_init("t1");

    field_desc_t *fd1 = fd_int_init("age");
    result = add_field(sch, fd1);

    field_desc_t *fd2 = fd_str_init("name", 12);
    result = add_field(sch, fd2);

    field_desc_t *fd3 = fd_int_init("numsiblings");
    result = add_field(sch, fd3);

    generate_table(sch);
    insert_ten(sch->first_block, 10);
    print_blocks(sch);

    const char *attributes[sch->field_amount];  
    char buf[256];  // Buffer large enough for input
    char temp[256];
    int count = 0;
    int project_all = 0;
    int selection_enabled = 0;
    while (1) {
        printf("$ ");
        fgets(buf, sizeof(buf), stdin);  // Read full line
        strcpy(temp, buf);
        count = 0;
        project_all = 0;

        char *token = strtok(temp, " ,");  // Operator/s: SELECT, INSERT, etc.

        if (strcmp(token, "SELECT") == 0) { 
            // Projection
            token = strtok(NULL, " ,"); // Attribute/s: age, SSN, etc
            printf("should be first attribute, is %s\n", token);
            // Check if projecting all attributes
            if (strcmp(token, "*") == 0) {
                project_all = 1;
                token = strtok(NULL, " ");
            } else {
                // Collect specific attributes
                while (token && strcmp(token, "FROM") != 0) {
                    attributes[count++] = token;
                    printf("should be attribute, is %s\n", token);
                    token = strtok(NULL, " ,");
                }
            }            
            // FROM
            
                        printf("should be FROM, is %s\n", token);

            // Schema name: t1, t2, etc
            token = strtok(NULL, " ");
                    printf("should be schema name, is %s\n", token);

            const char *schema_name = token;

            // Check for WHERE clause
            char *attribute = NULL;
            char *operator = NULL;
            char *value = NULL;

            token = strtok(NULL, " "); // WHERE
            printf("should be WHERE, is %s\n", token);
            
            if (token && strcmp(token, "WHERE") == 0) {
                
                attribute = strtok(NULL, " "); // attribute: age, SSN
                printf("attribute got is %s\n", attribute);
                selection_enabled = 1;

                operator = strtok(NULL, " "); // operator: <, =, >=
                printf("operator got is %s\n", operator);
                value = strtok(NULL, " ");    // value: attribute < value
                printf("value got is %s or in int %d\n", value, atoi(value));
            }
            if (project_all) {
                // Collect all attributes in schema
                int i = 0;
                for(field_desc_t *fd = sch->first; fd; fd = fd->next, i++){
                    attributes[i] = fd->name;
                }
                count = sch->field_amount;
            }

            // Perform selection, then projection
            if(selection_enabled){
                cmpfunc_t func = determine_op(operator);
                schema_t *selected_schema = selection(sch, attributes, count, attribute, atoi(value), func);
                project(selected_schema, attributes, count);
                continue;
              
            }
            project(sch, attributes, count);
                   
        } else {
            printf("%s: Unknown Command or Not Implemented Yet\n", token);
        }
    }
}
