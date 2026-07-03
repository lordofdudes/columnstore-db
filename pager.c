#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include "pager.h"
#include "storage.h"

#define PAGE_SIZE 4096

page_t pages[NUM_PAGES];


void print_page(page_t *page){
    printf("Page %d:\n", page->page_nr);
    
    for(int i = 0; i < PAGE_SIZE; i++){
        if(i % 16 == 0) printf("[%4d] ", i);  // row label at START of each row, fixed width

        unsigned char byte = (unsigned char)page->content[i];
        char display = isprint(byte) ? (char)byte : ' ';
        printf("%02x(%c) ", byte, display);

        if((i + 1) % 4 == 0) printf(" ");      // group separator every 4 bytes

        if((i + 1) % 16 == 0) printf("\n");    // newline at end of row
    }
    printf("\n");
}

page_t *pager_retrieve(int page_nr){
    printf("NUM_PAGES: %d\n", NUM_PAGES);
    for(int i = 0; i < NUM_PAGES; i++){
        printf("Checking page %d: page_nr=%d, new=%d\n", i, pages[i].page_nr, pages[i].new);
        if(pages[i].page_nr == page_nr && pages[i].new != 0){
            printf("retrieve_page: Page %d found in memory\n", page_nr);
            return &pages[i];
        }
    }
    printf("retrieve_page: Page %d not found in memory\n", page_nr);
    return NULL;
}

page_t *pager_get_available_page(){
    for(int i = 0; i < NUM_PAGES; i++){
        if(pages[i].pinned == 0){
            return &pages[i];
        }
    }
    printf("No unpinned pages available\n");
    return NULL;
}

int page_pin(int page_nr){
    for(int i = 0; i < NUM_PAGES; i++){
        if(pages[i].page_nr == page_nr){
            pages[i].pinned = 1;
            return 1;
        }
    }
    return 0;
}



void pager_reconstruct_schema(int fd){
    printf("Reconstructing schema\n");
    print_schema(&sch);
    int schema_start = file_size - 0x8 - footer_size;
    pager_read(fd, schema_start, &sch, sizeof(schema_t));
    print_schema(&sch);
}

void pager_reconstruct_field_descriptors(int fd, int start_page, int end_page){
    printf("Reconstructing field descriptors from footer pages %d to %d\n", start_page, end_page);
    int field_desc_start = file_size - 0x8 - footer_size + sizeof(schema_t);
    field_desc_t *prev = NULL;
    for(int i = 0; i < sch.field_amount; i++){
        field_desc_t *field = malloc(sizeof(field_desc_t));
        pager_read(fd, field_desc_start + i * (sizeof(field_desc_t) - 0x8), field, sizeof(field_desc_t) - 0x8);
        field->next = NULL;
        if(prev == NULL) head = field;
        else prev->next = field;
        prev = field;

        total_fields_size += field->size;
    }
    print_field_descriptors(head);
}

void pager_insert_record(int fd, record rc){
    printf("Before insertion of new row group\n");
    print_page(&pages[0]);
    if(sch.record_amount % sch.max_rg_record_amount == 0 && sch.record_amount != 0){
        // Make space for new row group 
        sch.record_amount++;
        pager_write_new_row_group(fd);
    }

    int available_row_group = sch.record_amount / sch.max_rg_record_amount;
    int available_row_group_offset = ((total_fields_size * sch.max_rg_record_amount) * available_row_group) + 4;
    printf("available rg %d from record amount %d and max rg record amount %d\n", available_row_group, sch.record_amount, sch.max_rg_record_amount);
    printf("total row group size %d\n", total_fields_size * sch.max_rg_record_amount);
    printf("available offset %d\n", available_row_group_offset);

    int i = 0;
    for(field_desc_t *cur = head; cur; cur = cur->next, i++){
        int slot_offset = available_row_group_offset + cur->size * (sch.record_amount % sch.max_rg_record_amount);
        printf("writing at %d\n", slot_offset);
        pager_write(fd, slot_offset, rc[i], cur->size);
        available_row_group_offset += cur->size * sch.max_rg_record_amount;
    }

    sch.record_amount++;
    int schema_start = file_size - 0x8 - footer_size;
    pager_write(fd, schema_start, &sch.record_amount, sizeof(int));
    printf("After insertion of new row group and record\n");
    print_page(&pages[0]);

    pager_flush(fd, &pages[0]);
}


void pager_write_new_row_group(int fd){
    int old_footer_start = file_size - 0x8 - footer_size;
    int total_row_group_size = 0;

    // Calculate new footer start based on old footer start + total row group size
    for(field_desc_t *cur = head; cur; cur = cur->next)
        total_row_group_size += cur->size * sch.max_rg_record_amount;
    
    int new_footer_start = old_footer_start + total_row_group_size;
    printf("Need to make new row group, moving footer from %d to %d\n", old_footer_start, new_footer_start);
    
    int cur_footer_size = 0;
    // Write schema to new location
    pager_write(fd, new_footer_start, &sch, sizeof(schema_t));
    cur_footer_size += sizeof(schema_t);

    // Write field descriptors to new location
    for(field_desc_t *cur = head; cur; cur = cur->next){
        pager_write(fd, new_footer_start + sizeof(schema_t) + (cur->ColumnID * (sizeof(field_desc_t) - 0x8)), cur, sizeof(field_desc_t) - 0x8);
        cur_footer_size += sizeof(field_desc_t) - 0x8;
    }

    // Cheap solution, recalculate column offsets for old + new row groups all over again
    int num_row_groups = (sch.record_amount / sch.max_rg_record_amount) + 1;
    for (int rg = 0; rg < num_row_groups + 1; rg++) {
        int col_offset = 4 + (rg * total_row_group_size);  // 4 = magic header
        for (field_desc_t *cur = head; cur; cur = cur->next) {
            pager_write(fd, new_footer_start + cur_footer_size, &col_offset, sizeof(int));
            cur_footer_size += sizeof(int);
            col_offset += cur->size * sch.max_rg_record_amount;
        }
    }

    // Write footer size to new location
    pager_write(fd, new_footer_start + cur_footer_size, &cur_footer_size, sizeof(int));
    footer_size = cur_footer_size;
    file_size += total_row_group_size + (sch.field_amount * sizeof(int)); // Update file size to include new row group + column offsets + footer size
    // Write magic number to new location
    pager_write(fd, new_footer_start + cur_footer_size + sizeof(int), "SAM1", 4);
}

// Loads footer metadata (footer size + schema + field descriptors) into footer size, schema and field descriptor variables.
int pager_read_footer(int fd){;
    if(file_size == 0) file_size = get_file_size(fd);

    // Read footer size, which is schema + field descriptors footer in bytes
    storage_read(fd, file_size - 0x8, &footer_size, sizeof(int));

    // Calculate footer page(s) and read schema + field descriptors + footer_size
    int start_footer_page = (file_size - 0x8 - footer_size) / PAGE_SIZE; 
    int end_footer_page = (file_size - 0x4 - 0x1)  / PAGE_SIZE;
    printf("Footer starts at page %d and ends at page %d\n", start_footer_page, end_footer_page);

    for(int page_num = start_footer_page; page_num <= end_footer_page; page_num++){
        page_t *pg = pager_retrieve(page_num);
        if(pg == NULL){
            pg = pager_get_available_page();
            // Initialize page metadata
            pg->page_nr = page_num;
            page_pin(page_num);
            pg->content = calloc(PAGE_SIZE, 1);
            pg->new = 1;
            // Read page's corresponding content from file
            int page_start = page_num * PAGE_SIZE;
            int bytes_read = storage_read(fd, page_start, pg->content, PAGE_SIZE);
            pg->valid_bytes = bytes_read;
        } else {
            printf("Metadata Page Error: Page %d already loaded in memory\n", page_num);
            return 0;
        }
    }
    printf("Reconstructing schema from footer pages %d to %d\n", start_footer_page, end_footer_page);
    pager_reconstruct_schema(fd);
    pager_reconstruct_field_descriptors(fd, start_footer_page, end_footer_page);
    return 1;
}   

int pager_insert_row(char *filename, char **col_vals) {
    int fd = open_file(filename);
    if(!fd){ printf("FD init failed\n"); return 0; }

    lock_file(fd);
    if(!validate_file(fd)){ unlock_file(fd); return 0; }

    // If schema doesn't exist, i.e. footer pages are not allocated
    // allocate corresponding schema + field descriptor + footer_size pages.
    printf("Checking if schema exists\n");
    if(sch.field_amount == 0) { printf("schema does not exist\n"); pager_read_footer(fd); }

    record rc = init_record(&sch, head, col_vals);
    print_record(rc);
    pager_insert_record(fd, rc);

    unlock_file(fd);
    close(fd);
    return 1;
}


/*
 * Reads `size` bytes starting at absolute file offset `start_addr` into `dest`,
 * transparently crossing as many pages as necessary. Loads any page not
 * already cached. Caller owns `dest` (must be pre-allocated, size >= `size`).
 * NOTE: This function copies from pages into `dest`, and should not be used to write to pages.
 */
void pager_read(int fd, int start_addr, void *dest, int size) {
    int bytes_copied = 0;

    if(size <= 0) { printf("PAGER READ ERROR: Reading size <= 0\n"); return; }
    if(dest == NULL) { printf("PAGER READ ERROR: Reading into NULL buffer\n"); return; }

    while (bytes_copied < size) {
        int current_addr = start_addr + bytes_copied;
        int page_num      = current_addr / PAGE_SIZE;
        int page_offset   = current_addr % PAGE_SIZE;

        page_t *pg = pager_retrieve(page_num);
        if(pg == NULL){
            // Will not be accessed when setting up footer pages as they are loaded beforehand
            // but needs to be potentially evicted and loaded in any other use-case
            printf("TODO Page %d: Fix eviction and loading of new pages.\n", page_num);
        }

        int bytes_left_in_page = pg->valid_bytes - page_offset;
        int bytes_left_to_read = size - bytes_copied;
        int chunk = (bytes_left_in_page < bytes_left_to_read)
                        ? bytes_left_in_page
                        : bytes_left_to_read;

        memcpy((unsigned char *)dest + bytes_copied, pg->content + page_offset, chunk);

        bytes_copied += chunk;
    }
}

/*
 * Writes `size` bytes from `src` to absolute page offset `start_addr`,
 * transparently crossing as many pages as necessary. Loads any page not
 * already cached. Caller owns `src` (must be pre-allocated, size >= `size`).
 * NOTE: This function copies from `src` into pages, and should not be used to read from pages.
 */
void pager_write(int fd, int start_addr, void *src, int size){
    int bytes_written = 0;

    if(size <= 0) { printf("PAGER WRITE ERROR: Writing size <= 0\n"); return; }
    if(src == NULL) { printf("PAGER WRITE ERROR: Writing NULL buffer\n"); return; }

    while (bytes_written < size) {
        int current_addr = start_addr + bytes_written;
        int page_num     = current_addr / PAGE_SIZE;
        int page_offset  = current_addr % PAGE_SIZE;

        page_t *pg = pager_retrieve(page_num);
        if(pg == NULL){
            printf("TODO Page %d: Fix eviction and loading of new pages.\n", page_num);
        }

        int bytes_left_in_page = PAGE_SIZE - page_offset;
        int bytes_left_to_write = size - bytes_written;
        int chunk = (bytes_left_in_page < bytes_left_to_write)
                        ? bytes_left_in_page
                        : bytes_left_to_write;

        memcpy(pg->content + page_offset, (unsigned char *)src + bytes_written, chunk);
        pg->dirty = 1;

        bytes_written += chunk;
    }
}

// Flushes a single page to disk if it is dirty. Does not unpin the page.
void pager_flush(int fd, page_t *page) {
    if(page == NULL) {
        printf("pager_flush: page is NULL\n");
        return;
    }
    
    if(page->dirty){
        int start_addr = page->page_nr * PAGE_SIZE;
        storage_write(fd, start_addr, page->content, page->valid_bytes);
        page->dirty = 0;
    }
}

