#ifndef QUERY_H
#define QUERY_H

int query_insert_row(char *filename, char **col_vals);
int query_create_table(char *filename, char **field_args, int field_count);


#endif 