#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pager.h"
#include "storage.h"

#define PAGE_SIZE 8

page_t pages[NUM_PAGES];

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

int footer_size;

void pager_reconstruct_schema(int fd, int file_size){
    printf("Reconstructing schema\n");
    print_schema(&sch);
    int schema_start = file_size - 0x8 - footer_size;
    pager_memcpy(fd, schema_start, &sch, sizeof(schema_t));
    print_schema(&sch);
}
/*
void pager_reconstruct_field_descriptors(int fd, int file_size, int start_page, int end_page){
    printf("Reconstructing field descriptors from footer pages %d to %d\n", start_page, end_page);
    int field_desc_start = file_size - 0x8 - footer_size + sizeof(schema_t);
    int field_desc_page_start_offset = (field_desc_start % PAGE_SIZE);
    field_desc_t *fd = malloc(sizeof(field_desc_t));
    pager_reconstruct_single_field_descriptor();
}

// Fd_dst: the position in a given field descriptor to read into
// Page_num: the current page number the section of the fd_dst corresponds to
// Page_offset: the offset within the page to start reading from
// Bytes_remaining: the number of bytes left to read into fd_dst
// This function recursively calls itself if a field descriptor spans multiple pages, until the entire field descriptor is read
// Additionally this function assumes the page passed to function must be retrieved 
int pager_reconstruct_single_field_descriptor(void *fd_dst, int page_num, int page_offset, int bytes_remaining){
    page_t *pg = pager_retrieve(page_num);
}
*/
// Loads footer metadata (footer size + schema + field descriptors) into footer size, schema and field descriptor variables.
void pager_read_footer(int fd){
    printf("1\n");
    int file_size = get_file_size(fd);
    printf("2\n");

    // Read footer size, which is schema + field descriptors footer in bytes
    storage_read(fd, file_size - 0x8, &footer_size, sizeof(int));
    printf("3\n");

    // Calculate footer page(s) and read schema + field descriptors + footer_size
    int start_footer_page = (file_size - 0x8 - footer_size) / PAGE_SIZE; 
    int end_footer_page = (file_size - 0x4 - 0x1)  / PAGE_SIZE;
    printf("Footer starts at page %d and ends at page %d\n", start_footer_page, end_footer_page);

    for(int page_num = start_footer_page; page_num <= end_footer_page; page_num++){
        page_t *pg = pager_retrieve(page_num);
        if(pg == NULL){

            pg = pager_get_available_page();
            pg->page_nr = page_num;
            page_pin(page_num);
            pg->content = malloc(PAGE_SIZE);
            pg->new = 1;
            int page_start = page_num * PAGE_SIZE;
            int bytes_remaining_in_file = file_size - page_start;
            int bytes_to_read = (bytes_remaining_in_file < PAGE_SIZE) ? bytes_remaining_in_file : PAGE_SIZE;
            int bytes_read = storage_read(fd, page_start, pg->content, bytes_to_read);
            pg->valid_bytes = bytes_read;
        } else {
            printf("Metadata Page Error: Page %d already loaded in memory\n", page_num);
            return 0;
        }
    }
    printf("Reconstructing schema from footer pages %d to %d\n", start_footer_page, end_footer_page);
    pager_reconstruct_schema(fd, file_size);
    //pager_reconstruct_field_descriptors(fd, file_size, start_footer_page, end_footer_page);
    
}   

int pager_insert_row(char *filename, char **col_vals) {
    int fd = open_file(filename);
    if(!fd){ printf("FD init failed\n"); return 0; }

    lock_file(fd);
    if(!validate_file(fd)){ unlock_file(fd); return 0; }

    // If schema doesn't exist, i.e. footer pages are not allocated
    // allocate corresponding schema + field descriptor + footer_size pages.
    printf("Checking if schema exists\n");
    if(sch.field_amount == 0) { pager_read_footer(fd); }

}


/*
 * Reads `size` bytes starting at absolute file offset `start_addr` into `dest`,
 * transparently crossing as many pages as necessary. Loads any page not
 * already cached. Caller owns `dest` (must be pre-allocated, size >= `size`).
 */
void pager_memcpy(int fd, int start_addr, void *dest, int size) {
    int bytes_copied = 0;

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

