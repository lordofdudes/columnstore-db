#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "columnstore.h"
#include "pager.h"
#include "schema.h"

// ============================== Record functions ================================

record init_record(schema_t *sch){
    record new_record = malloc(sizeof(void *) * sch->field_amount);
    for(int i = 0; i < sch->field_amount; i++){
        new_record[i] = malloc(sizeof(int));
    }

    return new_record;
}

void release_record(record r) {
    for(int i = 0; i < NUM_FIELDS; i++){
        free(r[i]);
    }

  free(r);
  r = 0;
}

void assign_int_field(void const *field_p, int val) {
  *(int *)field_p = val;
}

void insert_record(schema_t *sch, record r){
    for(int i = 0; i < sch->field_amount; i++){
        insert_col_val(sch, i + 1, *(int *)r[i]);
    }
    sch->record_amount++;
}

int get_record(schema_t *sch, record rc){
    if(sch->last_accessed == MAX_ATTR_VALUES) return 0;
    int i = 0;

    for(field_desc_t *fd = sch->first; fd; fd = fd->next, i++){
        for(int j = 0; j < NUM_BLOCKS; j++){

            if(get_column_id(fd->ColumnID, &sch->blocks[j])){

                int *attr_ptr = GET_HEADER(&sch->blocks[j], FREESPACE + (4 * sch->last_accessed));

                *(int *)rc[i] = *attr_ptr;

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
                //printf("src[%d(%s): %d] -> dest[%d(%s): %d]\n", j, 
                *(int *)dest_rc[i] = *(int *)src_rc[j];
            }
        }
    }
}


// ============================ Auxiliary functions ==================================



void generate_table(schema_t *sch){

    record rec = init_record(sch);
    
    int j = 0;
    field_desc_t *tmp = NULL;

    for(int i = 0; i < MAX_ATTR_VALUES; i++){

        for(j = 0, tmp = sch->first; tmp; tmp = tmp->next, j++){

            assign_int_field(rec[j], rand() % 10000);

        }

        insert_record(sch, rec);

    }
    release_record(rec);    
}



void print_record(schema_t *sch, record rec) {
    int column_width = 5;  // Set fixed width for each column

    for (int i = 0; i < sch->field_amount; i++) {
        printf("| %-*d ", column_width, *(int *)rec[i]);  // Left-align values
    }
    printf("|\n");
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
    else if(strcmp(op, "<") == 0){
        return int_greater_than;
    }
    else if(strcmp(op, "<=") == 0){ 
        return int_greater_than_or_equal;
    }
    else if(strcmp(op, ">") == 0){ 
        return int_less_than;
    }
    else if(strcmp(op, ">=") == 0){
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
        while(get_record(sch, rc1)){
            print_record(sch, rc1);
        }
        for (int l = 0; l < NUM_BLOCKS; l++)
            {
                print_block(&sch->blocks[l]);
            }
        release_record(rc1);
        return;
    }

    schema_t *projection_sch = make_sub_schema(sch, attr_count, attributes);
    record rc2 = init_record(sch);
    record sub_record = init_record(projection_sch);
    while(get_record(sch, rc2)){
        fill_sub_record(sch, rc2, projection_sch, sub_record);
        insert_record(projection_sch, sub_record);
    }
    while(get_record(projection_sch, sub_record)){
        print_record(projection_sch, sub_record);
    }
    
                    printf("\n\nresulting schema\n\n");

    for (int l = 0; l < 2; l++)
            {
                print_block(&projection_sch->blocks[l]);
            }


    release_record(sub_record);

}

schema_t *selection(schema_t *sch, const char *fields[], int count,
char *conditional, int value, cmpfunc_t func){
    int i = 0;
    schema_t *selected_schema = make_sub_schema(sch, count, fields);
    record src_rc = init_record(sch);
    record dest_rc = init_record(selected_schema);
    for(field_desc_t *fd = sch->first; fd; fd = fd->next, i++){
        if(strcmp(fd->name, conditional) == 0){
            for(int j = 0; j < MAX_ATTR_VALUES; j++){
                if(compare_val(&sch->blocks[i], func, value, j)){
                    printf("Is correct\n");
                    get_record(sch, src_rc);
                    fill_sub_record(sch, src_rc, selected_schema, dest_rc);
                    insert_record(selected_schema, dest_rc);
                }
            }

        }
    }
    release_record(src_rc);
    release_record(dest_rc);
    return selected_schema;
}

// ============================== Main loop/Interpreter ==========================================

// SELECT * FROM t1 WHERE age = 10

int main() {
    srand(time(NULL));
    //init_pages();
    int result;

    schema_t *sch = schema_init("t1", 3);

    field_desc_t *fd1 = fd_init("age", sizeof(int));
    result = add_field(sch, fd1);

    field_desc_t *fd2 = fd_init("SSN", sizeof(int));
    result = add_field(sch, fd2);

    field_desc_t *fd3 = fd_init("numsiblings", sizeof(int));
    result = add_field(sch, fd3);

    generate_table(sch);
    insert_ten(&sch->blocks[0], 10);

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
                printf("\n\noriginal schema\n\n");
            
    for (int l = 0; l < NUM_BLOCKS; l++)
            {
                print_block(&sch->blocks[l]);
            }
        
                return 1;
              
            }
            project(sch, attributes, count);
            printf("\n\noriginal schema\n\n");
            
    for (int l = 0; l < NUM_BLOCKS; l++)
            {
                print_block(&sch->blocks[l]);
            }
        
        } else {
            printf("Unknown Command or Not Implemented Yet\n");
        }
    }
}
