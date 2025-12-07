#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#define MAX_RG_RECORD_AMOUNT 16
// Size (in bytes) of pages
#define BLOCK_SIZE 256
// Number of allowed pages
#define NUM_PAGES 2

typedef int (*cmpfunc_t)(int, int);
typedef void ** record;

struct field_desc;
typedef struct field_desc field_desc_t;
typedef struct page page_t;

struct schema;
typedef struct schema schema_t;

struct field_desc{
    int size;
    char name[20];
    int type;
    int ColumnID;
    struct field_desc *next;
};

struct schema{
  int record_amount;  
  int max_rg_record_amount;
  int field_amount;
  //field_desc_t *first;
};


struct page{
  char *content;
  int page_nr;
  int last_accessed;
  int dirty;
  int pinned;
};

page_t *pages[NUM_PAGES];

int open_file(char *fname);
int insert_magic(int fd);
void insert_footer(int fd, schema_t *sch, field_desc_t *head); 
void init_row_group(int fd, schema_t *sch, field_desc_t *head);
void init_column(int fd, schema_t *sch, int columnSize);
int calculate_column_offset(int totalRecords, int totalPreviousColumnSizes, int max_rg_size);
int insert_row(char *filename, char **col_vals);
int validate_file(int fd);
void print_schema(schema_t *sch);
void reconstruct_schema(int fd, schema_t *sch, field_desc_t **head);
void insert_record(int fd, record rc, field_desc_t *head, schema_t *sch, int total_row_size);
field_desc_t *field_desc_init(int size, char *name, int type, int columnID);
int insert_field(field_desc_t *head, field_desc_t *insert);
schema_t *create_initial_schema(int field_amount, int max_rg_record_amount);
record init_record(schema_t *sch, field_desc_t *head, char **col_vals);
void lock_file(int fd);
void unlock_file(int fd);
record init_empty_record(schema_t *sch, field_desc_t *head);
char **return_all(int *outgoing_row_amount);
void *read_chunk(int fd, size_t size);
int find_file(char *fname);
void increment_record_amount(int fd, int record_amount);
char **project(schema_t *sch, field_desc_t *head, char **vals, int num_vals, char **cols, int num_cols);  
char **parse_query(char **cols, int num_cols, char *filtered_col, int amount, int *res2, char *op);
char **filter(int fd, schema_t *sch, field_desc_t *head, char *filtered_col, int amount, int *ptr2, char *op);
int parse_ints(void *chunk, int num_vals, int val, cmpfunc_t cmp_op);
cmpfunc_t determine_op(char *op);
void paginate_schema(schema_t *sch, field_desc_t *head, int offset);
void print_hex_dump(char *buffer, size_t length);

// Is functional starting point for database creation
// Sets up schema according to arguments and creates schema, with field descriptors and creates the initial file
int main(int argc, void *argv[]){
   if(argc < 2){
      printf("Usage: %s field:type:size [...], got %d %s\n", (char *)argv[0], argc, (char *)argv[1]);  
      return 1;
  }
    int field_amount = argc - 1;
    schema_t *sch = create_initial_schema(field_amount, MAX_RG_RECORD_AMOUNT);
    field_desc_t *head = NULL;
    field_desc_t *last = NULL;

    for (int i = 1; i < argc; i++) {
        char name[21] = {0};
        int type, size;

        // Parse "name:type:size"
        sscanf(argv[i], "%20[^:]:%d:%d", name, &type, &size);
        field_desc_t *fd = field_desc_init(size, name, type, i - 1);

        if (!head) {
            head = fd;
        } else {
            last->next = fd;
        }
        last = fd;
    }
  int fd = open_file("new_file1");
  if(!fd){
    printf("FD init failed\n");
    return 0;
  }

  if(!insert_magic(fd)){
    printf("Failed writing header magic\n");
    return 0;
  }
  init_row_group(fd, sch, head);
  insert_footer(fd, sch, head);

  if(!insert_magic(fd)){
    printf("Failed writing footer magic\n");
    return 0;
  }

  
  return 1;
}


// Attempts to open a file, if the file does not exist, create a new one
int open_file(char *fname){
    int fd = open(fname, O_RDWR, 0);
    
    // If file does not exist, create new one
    if(fd == -1) {
      if((fd = creat(fname, 0600)) == -1) {
          printf("Creation failed\n");
          return 0;
      }

      if(close(fd) == -1 || (fd = open(fname, O_RDWR, 0)) == -1) return 0;
      
  }

  return fd;
}

// Finds file descriptor
int find_file(char *fname){
  int fd = open(fname, O_RDWR, 0);

  if(fd == -1){
    return 0;
  }
  return fd;
}

// Inserts magic number "SAM1" at given position in file stream
int insert_magic(int fd){
  char *magic = "SAM1";
  int size = write(fd, magic, 4);
  if(size < 4){
    return 0;
  }
  
  return 1;
}

// Gathers information about schema and inserts footer information at end of file.
void insert_footer(int fd, schema_t *sch, field_desc_t *head){  
  lseek(fd, 0, SEEK_END);
  int res, footer_size = 0, zero = 0, one = 1, max_rg_size = 0;
  // Insert total amount of records
  res = write(fd, &sch->record_amount, sizeof(int));
  footer_size += sizeof(int);

  // Insert max row group column chunk record amount
  res = write(fd, &sch->max_rg_record_amount, sizeof(int));
  footer_size += sizeof(int);

  // Insert field amount
  res = write(fd, &sch->field_amount, sizeof(int));
  footer_size += sizeof(int);

  // Insert fields with size, name, type, id
  int totalPreviousSizes = 0;
  for(field_desc_t *cur = head; cur; cur = cur->next){
    max_rg_size += cur->size * sch->max_rg_record_amount;
    write(fd, &cur->size, sizeof(int)); // size
    footer_size += sizeof(int);

    write(fd, &cur->name, (size_t)20); // name
    footer_size += 20;

    if(cur->type == 0) write(fd, &zero, sizeof(int)); // type, INT
    else write(fd, &one, sizeof(int)); // STR max 20 bytes
    footer_size += sizeof(int);


    write(fd, &cur->ColumnID, sizeof(int)); // columnID
    footer_size += sizeof(int);
 
  }
  int row_group_num = sch->record_amount / sch->max_rg_record_amount;
  int i = 0;
  do{
    // Calculate and insert column offset and record amount
    for(field_desc_t *cur = head; cur; cur = cur->next){
      int offset = calculate_column_offset(sch->record_amount, totalPreviousSizes, max_rg_size);
      write(fd, &offset, sizeof(int));
      footer_size += sizeof(int);

      totalPreviousSizes += cur->size;
    }
    totalPreviousSizes = 0;
    i++;
  }while(i < row_group_num);


  printf("footer size %d WHAT IS WRONG\n", footer_size);
  // Insert footer size
  write(fd, &footer_size, sizeof(int));
}

/* 
  Writes/initializes a single row group, ie writes zeroes
  up to the sum of all column sizes,
  for example a schema with 4 INT fields and row-group max record amount = 16
  this function writes 16*16 zeroes
*/ 
void init_row_group(int fd, schema_t *sch, field_desc_t *head){
  // Initializes each separate column
  for(field_desc_t *tmp = head; tmp; tmp = tmp->next){
    init_column(fd, sch, tmp->size);
  }
}

/*
  Initializes/writes the empty area for a corresponding column
  fd: File to write in
  columnSize: Size of field in column (given in bytes)
  NOTE: uses the row group max record amount to allocate out the total amount of 
  bytes to allocate.
*/
void init_column(int fd, schema_t *sch, int columnSize){
  char zeroes[256] = {0};
  int totalBytes = columnSize * sch->max_rg_record_amount;
  while(totalBytes > 0){
    int chunk = (totalBytes > sizeof(zeroes)) ? sizeof(zeroes) : totalBytes;
    write(fd, zeroes, chunk);
    totalBytes -= chunk;
  }
  
}
// Calculates the beginning offset of each column chunk
int calculate_column_offset(int totalRecords, int totalPreviousColumnSizes, int max_rg_size){
  int row_group_id = totalRecords / MAX_RG_RECORD_AMOUNT;
  int row_group_offset = row_group_id * max_rg_size;
  int result = 4 + row_group_offset + (totalPreviousColumnSizes * MAX_RG_RECORD_AMOUNT);
  printf("calculated %d\n", result);
  return result;
}

/// @brief Creates record instance to insert into database
/// @param filename filename to insert row into
/// @param col_vals array of column values in row
int insert_row(char *filename, char **col_vals){
    
  int fd = open_file(filename);
  if(!fd){
    printf("FD init failed\n");
    return 0;
  }

  lock_file(fd);

  if(!validate_file(fd)){
    unlock_file(fd);
    return 0;
  }

  int footer_size;
  schema_t sch;
  field_desc_t *head = NULL;

  // Seek to footer size (past Magic Number)
  lseek(fd, -0x8, SEEK_END);
  // Read footer size
  int res = read(fd, &footer_size, sizeof(int));

  // Seek to top of footer
  lseek(fd, -0x8-footer_size, SEEK_END);
  reconstruct_schema(fd, &sch, &head);
  
  int total_row_group_size = 0;
  field_desc_t *cur = NULL;
  for(cur = head; cur; cur = cur->next){
    total_row_group_size += cur->size * sch.max_rg_record_amount;
  }

  record rc = init_record(&sch, head, col_vals);
  insert_record(fd, rc, head, &sch, total_row_group_size);

  unlock_file(fd);
  return 1;
}


// Actually does the insertion of the row into the database
void insert_record(int fd, record rc, field_desc_t *head, schema_t *sch, int total_row_size){
  int thingy = 0;
  if(sch->record_amount % sch->max_rg_record_amount == 0 && sch->record_amount != 0){
    printf("need ot make new block\n");
    int footer_size;
    // Seek to footer size (past Magic Number)
    lseek(fd, -0x8, SEEK_END);

    // Read footer size
    int res = read(fd, &footer_size, sizeof(int));

    // Seek to top of footer
    lseek(fd, -0x8-footer_size, SEEK_END);
    init_row_group(fd, sch, head);
    thingy = 1;
  }

  int available_row_group = sch->record_amount / sch->max_rg_record_amount;
  int available_row_group_offset = total_row_size * available_row_group + 4;
  printf("availabe rg %d from record amount %d and max rg record amount %d\n", available_row_group, sch->record_amount, sch->max_rg_record_amount);
  printf("total row group size %d\n", total_row_size);
  printf("availabel offset %d\n", available_row_group_offset);

  int base_column_offset = available_row_group_offset;
  int i = 0;
  for(field_desc_t *cur = head; cur; cur = cur->next, i++){
    printf("writing at %d\n", base_column_offset + cur->size * (sch->record_amount % sch->max_rg_record_amount));
    lseek(fd, base_column_offset + cur->size * (sch->record_amount % sch->max_rg_record_amount), SEEK_SET);
    write(fd, rc[i], cur->size);
    base_column_offset += cur->size * sch->max_rg_record_amount;
  }
  if(thingy){
    sch->record_amount++;
    insert_footer(fd, sch, head);
    insert_magic(fd);
    return;
  }
  sch->record_amount++;
  increment_record_amount(fd, sch->record_amount);
}

void print_schema(schema_t *sch){
  printf("SCHEMA: record amount %d, max record amount %d, field amount %d\n", sch->record_amount, sch->max_rg_record_amount, sch->field_amount);
}

// Reads footer in given file fd and extracts schema metadata into schema instance
void reconstruct_schema(int fd, schema_t *sch, field_desc_t **head){

  // Read schema info
  int res = read(fd, sch, sizeof(schema_t));
  print_schema(sch);

  // Read field information
  field_desc_t *prev = NULL;
  for(int i = 0; i < sch->field_amount; i++){
    field_desc_t *field = malloc(sizeof(field_desc_t));
    read(fd, field, sizeof(field_desc_t) - 8);
    field->next = NULL;

  if(prev == NULL){
      *head = field; 
  }else prev->next = field;
  
    prev = field;
  }

}

// Actually creates instance of record struct and inserts column values 
record init_record(schema_t *sch, field_desc_t *head, char **col_vals) {
    record rc = malloc(sizeof(void *) * sch->field_amount);
    int i = 0;

    for(field_desc_t *fd = head; fd; fd = fd->next, i++) {
        rc[i] = calloc(1, fd->size); 

        if(fd->type == 0) { // INT
            int val = atoi(col_vals[i]);
            memcpy(rc[i], &val, sizeof(int));
        } 
        else{ // STRING
            // Limit the copy to field size - 1 and null-terminate
            strncpy((char *)rc[i], col_vals[i], fd->size - 1);
            ((char *)rc[i])[fd->size - 1] = '\0';
        }
    }

    return rc;
}

// To be used when retrieving rows from database
record init_empty_record(schema_t *sch, field_desc_t *head){
  record rc = malloc(sch->field_amount * sizeof(void *));
  int i = 0;
  for(field_desc_t *fd = head; fd; fd = fd->next, i++) rc[i] = malloc(fd->size);
  return rc;
}

// Schema struct
schema_t *create_initial_schema(int field_amount, int max_rg_record_amount){
  schema_t *sch = malloc(sizeof(schema_t));
  sch->field_amount = field_amount;
  sch->max_rg_record_amount = max_rg_record_amount;
  sch->record_amount = 0;
  return sch;
}

// Initializes field descriptors
field_desc_t *field_desc_init(int size, char *name, int type, int columnID) {
    field_desc_t *fd = malloc(sizeof(field_desc_t));
 
    fd->size = size;

    memset(fd->name, 0, 20);
    // Copy up to 19 characters and ensure null-termination
    strncpy(fd->name, name, 19);
    fd->name[19] = '\0';  

    fd->type = type;
    fd->ColumnID = columnID;
    fd->next = NULL;

    return fd;
}

// Inserts a given field descriptor into the schema's field descriptor list
int insert_field(field_desc_t *head, field_desc_t *insert){
  if(head == NULL){
    return 1;
  }

  int i = 0;
  field_desc_t *tmp = head;
  while (tmp->next) {
      if (strncmp(tmp->name, insert->name, 20) == 0) {
          printf("%s already exists at position %d\n", tmp->name, i);
          return 0;
      }
      tmp = tmp->next;
      i++;
  }

  // Check the last node as well
  if (strncmp(tmp->name, insert->name, 20) == 0) {
      printf("%s already exists at position %d\n", tmp->name, i);
      return 0;
  }

  tmp->next = insert;

}

// Checks if file is proper type, ie has correct magic number
int validate_file(int fd){
  char magic[4];
  lseek(fd, -0x4, SEEK_END);
  read(fd, magic, sizeof(magic));
  if(strcmp(magic, "SAM1") == 0){
    printf("Correct file format\n");
    return 1;
  }
  else{
    printf("Wrong file format\n");
    return 0;  
  }
}

// Locks file to avoid race conditions
void lock_file(int fd){
  struct flock lock = {0};
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;

  fcntl(fd, F_SETLKW, &lock);
}

// Unlocks file
void unlock_file(int fd){
  struct flock lock = {0};
  lock.l_type = F_UNLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;

  fcntl(fd, F_SETLK, &lock);
}



// Returns all columns of all rows UNUSED at this point
char **return_all(int *outgoing_row_amount) {
    int fd = find_file("new_file1");
    if (!fd) {
        printf("failed to find file\n");
        return NULL;
    }

    if(!validate_file(fd)){
      printf("Failed to validate file in return all\n");
      return NULL;
    }

    lseek(fd, -0x8, SEEK_END);
    int footer_size;
    read(fd, &footer_size, sizeof(int));
    printf("got footer size %d\n", footer_size); 

    schema_t sch;
    field_desc_t *head = NULL;
    lseek(fd, -0x8 - footer_size, SEEK_END);
    reconstruct_schema(fd, &sch, &head);

    if (sch.record_amount == 0) {
        return NULL;
    }

    *outgoing_row_amount = sch.record_amount;
    int offset;
    read(fd, &offset, sizeof(int));
    printf("before is at %d and offset %d\n", lseek(fd, 0, SEEK_CUR), offset);

    lseek(fd, offset, SEEK_SET);
    printf("after is at %d\n", lseek(fd, 0, SEEK_CUR));
    // Allocate only needed rows
    char **rows = malloc(sizeof(char *) * sch.record_amount);

    int total_sizes = 0;
    for (field_desc_t *fd = head; fd; fd = fd->next)
        total_sizes += fd->size + 1; // Add space for delimiters

    // Init empty strings
    for (int i = 0; i < sch.record_amount; i++) {
        rows[i] = calloc(1, total_sizes + 1);  // +1 for null terminator
    }

    for (field_desc_t *fid = head; fid; fid = fid->next) {
        void *chunk = read_chunk(fd, sch.max_rg_record_amount * fid->size);

        for (int i = 0; i < sch.record_amount; i++) {
            char temp[71] = {0};

            if (fid->type == 0) {
                int val;
                memcpy(&val, (char *)chunk + i * fid->size, sizeof(int));
                snprintf(temp, sizeof(temp), "%d", val);
            } else {
                strncpy(temp, (char *)chunk + i * fid->size, fid->size);
                temp[fid->size] = '\0';
            }

            strcat(rows[i], temp);
            strcat(rows[i], ",");  // delimiter
        }

        free(chunk);
    }

    return rows;
}


// Assume any read is aligned to start of column chunk
// fd: file to read
// size: number of bytes to read, given number of records and size of column
void *read_chunk(int fd, size_t size){
  if(size == 0){
    printf("Reading size 0\n");
    return NULL;
  }
  void *chunk = malloc(size);
  int size_read = read(fd, chunk, size);
  if(size_read != size){
    printf("read failed, to read %d, actually read %d\n", size, size_read);
    return NULL;
  }

  return chunk;
}

// Manually increments record amount in footer metadata
void increment_record_amount(int fd, int record_amount){
  lseek(fd, -0x8, SEEK_END);
  int footer_size;
  int res = read(fd, &footer_size, sizeof(int));

  lseek(fd, -0x8-footer_size, SEEK_END);
  write(fd, &record_amount, sizeof(int));
}


/// @brief Returns all rows corresponding to column given by **cols
/// @param cols strings with name of each column, ie "state", "date" etc.. 
/// @return list of row-converted column values

char **project(schema_t *sch, field_desc_t *head, char **vals, int num_vals, char **cols, int num_cols) {
    int num_rows = num_vals / sch->field_amount;
    char **arr = malloc(sizeof(char *) * (num_cols * num_rows));
    for(int row = 0; row < num_rows; row++){
        for(int c = 0; c < num_cols; c++){
            // Find ColumnID for this column name
            int col_id = -1;
            for(field_desc_t *fd = head; fd; fd = fd->next){
                if(strcmp(fd->name, cols[c]) == 0){
                    col_id = fd->ColumnID;
                    break;
                }
            }

            if(col_id == -1){
                printf("Column %s not found\n", cols[c]);
                continue;
            }

            arr[row * num_cols + c] = strdup(vals[row * sch->field_amount + col_id]);
            printf("got %s\n", arr[row * num_cols + c]);
        }
    }
    printf("finishing\n");
    return arr;
}


// Returns an array of all values in corresponding rows
// employs two-pass searching, ie one search to total the amount of correct rows
// second to actually retrieve them
char **filter(int fd, schema_t *sch, field_desc_t *head, char *filtered_col, int amount, int *ptr2, char *op){
// TODO: Add support for LIKE %s functionality
  cmpfunc_t cmp_op;
  if(!(cmp_op = determine_op(op))){
    printf("Invalid operator %s\n", op);
    return NULL;
  }

  int sum = 0, cur_size;
  int *col_offsets = malloc(sizeof(int) * sch->field_amount);
  for(field_desc_t *fd = head; fd; fd = fd->next) sum += fd->size; 

  int col_id = -1, current = 0, offset, idx = 0, final_offset;
  field_desc_t *cur = NULL;
  for(cur = head; cur; cur = cur->next, idx++){
    
    read(fd, &offset, sizeof(int));  
    col_offsets[idx] = offset;
    if(strcmp(cur->name, filtered_col) == 0){
      final_offset = offset;
      cur_size = cur->size;
      col_id = cur->ColumnID;
    }
  }
  offset = final_offset;

  if(col_id == -1){
    printf("Column %s does not exist in table\n", filtered_col);
    return NULL;
  }

  // Seeks to corresponding column chunk
  lseek(fd, offset, SEEK_SET);
  int remainder = sch->record_amount % sch->max_rg_record_amount;
  int group_records = (remainder == 0) ? sch->max_rg_record_amount : remainder;
  void *chunk = read_chunk(fd, cur_size * group_records);
  current += parse_ints(chunk, group_records, amount, cmp_op);

  int out_index = 0;
  char **arr = malloc(sizeof(char *) * sch->field_amount * current);

  lseek(fd, offset, SEEK_SET);
  int *ptr = (int *)chunk;

  for(int row = 0; row < group_records; row++){
    if(cmp_op(ptr[row], amount) == 1){
      field_desc_t *col = head;
      for(int c = 0; c < sch->field_amount; c++, col = col->next){
        lseek(fd, col_offsets[c] + row * col->size, SEEK_SET);
        void *buf = malloc(col->size);
        read(fd, buf, col->size);

        char tmp[71];
        if(col->type == 0){
          int v;
          memcpy(&v, buf, sizeof(int));
          snprintf(tmp, sizeof(tmp), "%d", v);
        } 
        else{
          snprintf(tmp, sizeof(tmp), "%s", (char *)buf);
        }
          arr[out_index++] = strdup(tmp);
          free(buf);
          *ptr2 += 1;
      }
    }
  }

  free(chunk);
  free(col_offsets);
  return arr;
}

char **parse_query(char **cols, int num_cols, char *filtered_col, int amount, int *res2, char *op){
  schema_t sch;
  field_desc_t *head = NULL;
  int num = 0;
  
  int fd = find_file("new_file1");
  
  if(!validate_file(fd)){
    printf("Wrong file type\n");
    return NULL;
  }
  int footer_size;
  lseek(fd, -0x8, SEEK_END);
  read(fd, &footer_size, sizeof(int));
  lseek(fd, -0x8-footer_size, SEEK_END);
  reconstruct_schema(fd, &sch, &head);

  char **vals = filter(fd, &sch, head, filtered_col, amount, &num, op);
  char **res = project(&sch, head, vals, num, cols, num_cols);
  *res2 = num_cols * (num / sch.field_amount);
  return res;
}


int parse_ints(void *chunk, int num_vals, int val, cmpfunc_t cmp_op){
  int *ptr = (int *)chunk;
  int num_rows = 0;
  for(int i = 0; i < num_vals; i++){
    if(cmp_op(ptr[i], val) == 1){
      num_rows++;
    }
  }
  return num_rows;
}

int int_equal(int x, int y){ return x == y; }
int int_not_equal(int x, int y){ return x != y; }
int int_less_than(int x, int y){ return x < y; }
int int_less_than_equal(int x, int y){ return x <= y; }
int int_greater_than(int x, int y){ return x > y; }
int int_greater_than_equal(int x, int y){ return x >= y; }

cmpfunc_t determine_op(char *op){
  if(strcmp(op, "=") == 0){
    return int_equal;
  }
  if(strcmp(op, "!=") == 0){
    return int_not_equal;
    }
  if(strcmp(op, "<") == 0){
    return int_less_than;
  }
  if(strcmp(op, "<=") == 0){
    return int_less_than_equal;
  }
  if(strcmp(op, ">") == 0){
    return int_greater_than;
  }
  if(strcmp(op, ">=") == 0){
    return int_greater_than_equal;
  }
  return NULL;
}

void print_hex_dump(char *buffer, size_t length) {
    for (size_t i = 0; i < length; i++) {
        printf("%02X ", buffer[i]); // Prints each byte as a two-digit hexadecimal number
    }
    printf("\n");
}
