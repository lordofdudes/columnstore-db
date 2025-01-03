#include "schema.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


field_desc_t *fd_init(char *name, size_t size){
    field_desc_t *new_fd = malloc(sizeof(field_desc_t));
    new_fd->name = name;
    new_fd->next = NULL;
    new_fd->ColumnID = 0;
    new_fd->size = size;
    return new_fd;
}

schema_t *schema_init(char *name, int num_blocks){
    schema_t *new_schema = malloc(sizeof(schema_t));
    new_schema->blocks = malloc(sizeof(block_t) * num_blocks);
    for(int i = 0; i < num_blocks; i++){
        init_block(&new_schema->blocks[i]);
        fill_page(&new_schema->blocks[i], i);
    }

    
    new_schema->first = NULL;
    new_schema->field_amount = 0;
    new_schema->sch_name = name;
    new_schema->last_accessed = 0;
    new_schema->record_amount = 0;
    return new_schema;
}

schema_t *make_sub_schema(schema_t *sch, int num_fields, const char *fields[]){
    char *sub_schema_name = "project";
    
    schema_t *project_sch = schema_init(sub_schema_name, num_fields);
  
    field_desc_t *fd = NULL;
    for(int i = 0; i < num_fields; i++){
        fd = get_field(sch, fields[i]);
        if(fd){
            printf("Added field %s to sub schema\n", fd->name);
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
    return 1;

}

field_desc_t *dup_field(field_desc_t *f, size_t size) {
  field_desc_t *res = malloc(sizeof(field_desc_t));
  res->name = strdup(f->name);
  res->next = NULL;
  res->ColumnID = 0;
  res->size = size;
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