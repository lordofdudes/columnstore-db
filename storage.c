#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "storage.h"
#include "schema.h"
#include <stdint.h>

#include "pager.h"

schema_t sch = {0};
field_desc_t *head = NULL;
int footer_size = 0;
int file_size = 0;

/* ------------------------------------------------------------------ */
/* File lifecycle                                                       */
/* ------------------------------------------------------------------ */

int open_file(char *fname) {
    int fd = open(fname, O_RDWR, 0);
    if (fd == -1) {
        if ((fd = creat(fname, 0600)) == -1) {
            printf("Creation failed\n");
            return 0;
        }
        if (close(fd) == -1 || (fd = open(fname, O_RDWR, 0)) == -1) return 0;
    }
    return fd;
}

int find_file(char *fname) {
    int fd = open(fname, O_RDWR, 0);
    if (fd == -1) return 0;
    return fd;
}

int validate_file(int fd) {
    char magic[4];
    lseek(fd, -0x4, SEEK_END);
    read(fd, magic, sizeof(magic));
    if (strcmp(magic, "SAM1") == 0) {
        printf("Correct file format\n");
        return 1;
    }
    printf("Wrong file format\n");
    return 0;
}

void lock_file(int fd) {
    struct flock lock = {0};
    lock.l_type   = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start  = 0;
    lock.l_len    = 0;
    fcntl(fd, F_SETLKW, &lock);
}

void unlock_file(int fd) {
    struct flock lock = {0};
    lock.l_type   = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start  = 0;
    lock.l_len    = 0;
    fcntl(fd, F_SETLK, &lock);
}

long get_file_size(int fd) {
    struct stat st;
    
    if (fstat(fd, &st) == 0) {
        // st.st_size holds the size in bytes (off_t type)
        return st.st_size;
    } else {
        perror("Failed to get file stat");
        return -1;
    }
}

/* ------------------------------------------------------------------ */
/* Magic number                                                         */
/* ------------------------------------------------------------------ */

int insert_magic(int fd) {
    char *magic = "SAM1";
    if (write(fd, magic, 4) < 4) return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Row group / column initialisation                                   */
/* ------------------------------------------------------------------ */

void init_column(int fd, schema_t *sch, int columnSize) {
    char zeroes[256] = {0};
    int totalBytes = columnSize * sch->max_rg_record_amount;
    while (totalBytes > 0) {
        int chunk = (totalBytes > (int)sizeof(zeroes)) ? (int)sizeof(zeroes) : totalBytes;
        write(fd, zeroes, chunk);
        totalBytes -= chunk;
    }
}

void init_row_group(int fd, schema_t *sch, field_desc_t *head) {
    for (field_desc_t *tmp = head; tmp; tmp = tmp->next)
        init_column(fd, sch, tmp->size);
}

/* ------------------------------------------------------------------ */
/* Footer                                                               */
/* ------------------------------------------------------------------ */

int calculate_column_offset(int totalRecords, int totalPreviousColumnSizes, int max_rg_size) {
    int row_group_id     = totalRecords / MAX_RG_RECORD_AMOUNT;
    int row_group_offset = row_group_id * max_rg_size;
    int result = 4 + row_group_offset + (totalPreviousColumnSizes * MAX_RG_RECORD_AMOUNT);
    printf("calculated %d\n", result);
    return result;
}

void insert_footer(int fd, schema_t *sch, field_desc_t *head) {
    lseek(fd, 0, SEEK_END);
    int footer_size = 0, zero = 0, one = 1, max_rg_size = 0;

    write(fd, &sch->record_amount,         sizeof(int)); footer_size += sizeof(int);
    write(fd, &sch->max_rg_record_amount,  sizeof(int)); footer_size += sizeof(int);
    write(fd, &sch->field_amount,          sizeof(int)); footer_size += sizeof(int);

    int totalPreviousSizes = 0;
    for (field_desc_t *cur = head; cur; cur = cur->next) {
        max_rg_size += cur->size * sch->max_rg_record_amount;
        write(fd, &cur->size,     sizeof(int)); footer_size += sizeof(int);
        write(fd, &cur->name,     20);          footer_size += 20;
        if (cur->type == 0) write(fd, &zero, sizeof(int));
        else                write(fd, &one,  sizeof(int));
        footer_size += sizeof(int);
        write(fd, &cur->ColumnID, sizeof(int)); footer_size += sizeof(int);
    }

    int row_group_num = sch->record_amount / sch->max_rg_record_amount;
    int i = 0;
    do {
        for (field_desc_t *cur = head; cur; cur = cur->next) {
            int offset = calculate_column_offset(sch->record_amount, totalPreviousSizes, max_rg_size);
            write(fd, &offset, sizeof(int));
            footer_size += sizeof(int);
            totalPreviousSizes += cur->size;
        }
        totalPreviousSizes = 0;
        i++;
    } while (i < row_group_num);

    printf("INSERT_FOOTER footer_size: %d\n", footer_size);
    write(fd, &footer_size, sizeof(int));
}

void increment_record_amount(int fd, int record_amount) {
    lseek(fd, -0x8, SEEK_END);
    int footer_size;
    read(fd, &footer_size, sizeof(int));
    lseek(fd, -0x8 - footer_size, SEEK_END);
    write(fd, &record_amount, sizeof(int));
}

/* ------------------------------------------------------------------ */
/* Record insertion                                                     */
/* ------------------------------------------------------------------ */

record init_record(schema_t *sch, field_desc_t *head, char **col_vals) {
    record rc = malloc(sizeof(void *) * sch->field_amount);
    int i = 0;
    for (field_desc_t *fd = head; fd; fd = fd->next, i++) {
        rc[i] = calloc(1, fd->size);
        if (fd->type == 0) {
            int val = atoi(col_vals[i]);
            memcpy(rc[i], &val, sizeof(int));
        } else {
            strncpy((char *)rc[i], col_vals[i], fd->size - 1);
            ((char *)rc[i])[fd->size - 1] = '\0';
        }
    }
    return rc;
}

record init_empty_record(schema_t *sch, field_desc_t *head) {
    record rc = malloc(sch->field_amount * sizeof(void *));
    int i = 0;
    for (field_desc_t *fd = head; fd; fd = fd->next, i++)
        rc[i] = malloc(fd->size);
    return rc;
}

void insert_record(int fd, record rc, field_desc_t *head, schema_t *sch, int total_row_size) {
    int new_row_group = 0;
    if (sch->record_amount % sch->max_rg_record_amount == 0 && sch->record_amount != 0) {
        printf("need to make new block\n");
        int footer_size;
        lseek(fd, -0x8, SEEK_END);
        read(fd, &footer_size, sizeof(int));
        lseek(fd, -0x8 - footer_size, SEEK_END);
        init_row_group(fd, sch, head);
        new_row_group = 1;
    }
 
    int available_row_group        = sch->record_amount / sch->max_rg_record_amount;
    int available_row_group_offset = total_row_size * available_row_group + 4;
    printf("available rg %d from record amount %d and max rg record amount %d\n", available_row_group, sch->record_amount, sch->max_rg_record_amount);
    printf("total row group size %d\n", total_row_size);
    printf("available offset %d\n", available_row_group_offset);

    int base_column_offset = available_row_group_offset;
    int i = 0;
    for (field_desc_t *cur = head; cur; cur = cur->next, i++) {
        int slot_offset = base_column_offset + cur->size * (sch->record_amount % sch->max_rg_record_amount);
        printf("writing at %d\n", slot_offset);
        lseek(fd, slot_offset, SEEK_SET);
        write(fd, rc[i], cur->size);
        base_column_offset += cur->size * sch->max_rg_record_amount;
    }

    sch->record_amount++;
    if (new_row_group) {
        insert_footer(fd, sch, head);
        insert_magic(fd);
    } else {
        increment_record_amount(fd, sch->record_amount);
    }
}

int insert_row(char *filename, char **col_vals) {
    //int fd = open_file(filename);
    //if (!fd) { printf("FD init failed\n"); return 0; }
    int fd = 0;
    //lock_file(fd);
    //if (!validate_file(fd)) { unlock_file(fd); return 0; }

    int footer_size;
    pager_insert_row(filename, col_vals);
    return 1;
    // Seek to top of footer size int segment 
    lseek(fd, -0x8, SEEK_END);
    // Read footer size into variable
    read(fd, &footer_size, sizeof(int));
    
    // Seek to start of footer using footer size
    lseek(fd, -0x8 - footer_size, SEEK_END);
    printf("footer size: %d\n", footer_size);
    // Reconstruct schema and field descriptors by reading footer (schema + field descriptor) metadata
    reconstruct_schema(fd, &sch, &head);
    printf("done reconstructing schema\n");
    // Calculate total row group size (sum of all field sizes * max records per row group)
    int total_row_group_size = 0;
    for(field_desc_t *cur = head; cur; cur = cur->next)
    total_row_group_size += cur->size * sch.max_rg_record_amount;
    
    // Initialize record with column values
    record rc = init_record(&sch, head, col_vals);

    // Insert record into file
    insert_record(fd, rc, head, &sch, total_row_group_size);

    unlock_file(fd);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Low-level helpers                                                    */
/* ------------------------------------------------------------------ */

void *read_chunk(int fd, size_t size) {
    if (size == 0) { printf("Reading size 0\n"); return NULL; }
    void *chunk = malloc(size);
    ssize_t n = read(fd, chunk, size);
    if ((size_t)n != size) {
        printf("read failed, to read %zu, actually read %zd\n", size, n);
        free(chunk);
        return NULL;
    }
    return chunk;
}

void print_hex_dump(char *buffer, size_t length) {
    for (size_t i = 0; i < length; i++)
        printf("%02X ", (unsigned char)buffer[i]);
    printf("\n");
}

// Because function uses SEEK_SET, the offset has to be 0-indexed.
// Reading the very first byte of the file would require offset = 0, reading the second byte would require offset = 1, etc.
int storage_read(int fd, int offset, void *buffer, int size){
    lseek(fd, offset, SEEK_SET);
    int n = read(fd, buffer, size);
    if (n < 0) {
        printf("storage_read failed, to read %d, actually read %d\n", size, n);
        return 0;  
    }
    return n;
}

int storage_write(int fd, int offset, void *buffer, int size){
    lseek(fd, offset, SEEK_END);
    int n = write(fd, buffer, size);
    if (n != size) {
        printf("storage_write failed, to write %d, actually wrote %d\n", size, n);
        return 0;  
    }
    return n;
}