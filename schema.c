#include "schema.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


field_desc_t *fd_int_init(char *name){
    field_desc_t *new_fd = malloc(sizeof(field_desc_t));
    new_fd->name = strdup(name);
    new_fd->next = NULL;
    new_fd->ColumnID = 0;
    new_fd->size = sizeof(int);
    new_fd->type = INT;
    return new_fd;
}

field_desc_t *fd_str_init(char *name, int size){
    field_desc_t *new_fd = malloc(sizeof(field_desc_t));
    new_fd->name = strdup(name);
    new_fd->next = NULL;
    new_fd->ColumnID = 0;
    new_fd->size = size;
    new_fd->type = STR;
    return new_fd;
}

schema_t *schema_init(char *name){
    schema_t *new_schema = malloc(sizeof(schema_t));
    new_schema->first_block = NULL; 
    new_schema->first = NULL;
    new_schema->field_amount = 0;
    new_schema->sch_name = name;
    new_schema->last_accessed = 0;
    new_schema->record_amount = 0;
    new_schema->num_blocks = 0;
    new_schema->current_blockID = 0;
    return new_schema;
}

schema_t *make_sub_schema(schema_t *sch, int num_fields, const char *fields[]){
    char *sub_schema_name = "project";
    
    schema_t *project_sch = schema_init(sub_schema_name);
  
    field_desc_t *fd = NULL;
    for(int i = 0; i < num_fields; i++){
        fd = get_field(sch, fields[i]);
        if(fd){
            add_field(project_sch, dup_field(fd, fd->size));
        }else{
            printf("Field %s is not present in schema\n", fields[i]);
            return NULL;
        }
    }

    return project_sch;
}


int add_field(schema_t *sch, field_desc_t *fd){
    int columnID = 1;
    // If schema has no fields, add as first field
    if(sch->first == NULL){
        if(fd->ColumnID == 0){
            fd->ColumnID = columnID;
        }
        sch->first = fd;
        sch->field_amount++;
        insert_block(sch);
        return 1;
    }

    if(strcmp(sch->first->name, fd->name) == 0) return -1; // Name already exists in schema

    // Iterate through each fd in schema
    field_desc_t *tmp = sch->first;
    while(tmp->next){
        tmp = tmp->next;
        columnID++;
        if(strcmp(tmp->name, fd->name) == 0) return -1; // Name already exists in schema
    }
    columnID++;
    // Reached end of fd-list, insert at end
    if(fd->ColumnID == 0){
        fd->ColumnID = columnID;
    }
    tmp->next = fd;
    sch->field_amount++;
    insert_block(sch);
    return 1;

}

field_desc_t *dup_field(field_desc_t *f, size_t size) {
  field_desc_t *res = malloc(sizeof(field_desc_t));
  res->name = strdup(f->name);
  res->next = NULL;
  res->ColumnID = 0;
  res->size = size;
  res->type = f->type;
  return res;
}

field_desc_t *get_field(schema_t *sch, const char *name){
    for(field_desc_t *fd = sch->first; fd; fd = fd->next){
        if(strcmp(fd->name, name) == 0){
            return fd;
        } 
    }
    return 0;
}

void insert_block(schema_t *sch){
    allocate_block(sch, sch->num_blocks);    
    sch->num_blocks++;
}

void allocate_block(schema_t *sch, int num_blocks) {
    if (!sch->first_block) {
        sch->first_block = malloc(sizeof(block_t));
        init_block(sch->first_block);
        fill_page(sch->first_block, num_blocks);
        return;
    }

    block_t *current = sch->first_block;
    while (current->next) {
        current = current->next;
    }

    block_t *new_block = malloc(sizeof(block_t));
    init_block(new_block);
    fill_page(new_block, num_blocks);
    current->next = new_block;
    printf("New block allocated: %p, linked to: %p\n", new_block, current);
}

void free_schema(schema_t *sch){
    // Free the linked list of fields
    field_desc_t *fd = sch->first;
    while (fd) {
        field_desc_t *next_fd = fd->next;
        free(fd);  
        fd = next_fd; 
    }

    // Free blocks
   // for (int i = 0; i < NUM_BLOCKS; i++) {
    //    if (sch->blocks[i].page_ptr != NULL) {
     //       free(sch->blocks[i].page_ptr);  // Free dynamically allocated page
      //      sch->blocks[i].page_ptr = NULL; 
       // }
   // }


}