#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "schema.h"
#include "storage.h"
#include "pager.h"
#include "query.h"


// This file, and in turn the main() function, functions as a CLI option for database management
// in contrast with for example the web application variant
// which utilizes a shared library to call functions in Flask.
// Note: a lot of additional functionalities like querying
// is currently only supported on the web application


void print_help(){
    printf("COMMANDS:\n");

    printf("  create table_name fieldname:type:size\n");
    printf("      where fieldname is the name of the field, type is either 0 for int or 1 for string\n");
    printf("      and size is the MAX number of bytes each field can occupy on disk\n\n");

    printf("  insert table_name colval colval colval ...\n");
    printf("      where colval is the value of each column in the record\n\n");

    printf("  Querying is currently supported only by use of the web application\n\n");
    printf("  More functionality to be added soon\n");
}

// Usage: argv[x] = fieldname:type:size(in bytes) [...] 0=int, 1=string
// Example: ./samdb create mytable id:0:4 name:1:20 age:0:4
int main(int argc, char *argv[]) {
    if(argc == 1){
        printf("Please enter command `./samdb help` for usage\n");
        return 1;
    } 

    if(strcmp(argv[1], "help") == 0){
        print_help();
        return 1;
    }
    printf("Enter %s help to get list of all commands and how to use them\n", argv[0]);
    if(argc < 3){
        printf("Usage: %s <command> <table> [args]\n", argv[0]);
        return 1;
    }
    char *cmd   = argv[1];
    char *table = argv[2];

    if(strcmp(cmd, "create") == 0){
        int res = query_create_table(table, argv + 3, argc - 3);
        if(res){
            printf("Created table %s with %d fields:\n", table, argc - 3);
            for(int i = 0; i < argc - 3; i++){
                printf("%s ", (argv + 3)[i]); 
            }
        } else printf("Failed to create table %s\n", table);
        return 1;
    } 
    
    if(strcmp(cmd, "insert") == 0){
        int res = query_insert_row(table, argv+3);
        if(res){
            printf("Inserted row: ");
            for(int i = 0; i < argc - 3; i++){
                printf("%s ", (argv+3)[i]);
            }
        } else printf("Failed to insert record\n");
        return 1;
    }

    // future: query, delete...
    printf("Unknown command: %s\n", cmd);
    return 0;
}

