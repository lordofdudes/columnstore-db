#include <stdio.h>
#include "pager.h"


/*
 Handles INSERT query, verifying and passing input to pager
    filename: Name of file to insert into.
    col_vals: Values to insert.
 NOTE: Expects each column value to be strings, so an int attribute 
       like 465 would be the string "465"
*/
int query_insert_row(char *filename, char **col_vals){
    char *FAIL = "INSERTION FAIL:";

    if(filename == NULL){
        printf("%s filename not provided\n", FAIL);
        return 0;
    }    

    if(col_vals == NULL){
        printf("%s no values provided", FAIL);
        return 0;
    }

    pager_insert_row(filename, col_vals);
    return 1;
}