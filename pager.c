#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include "pager.h"
#include "storage.h"

#define PAGE_SIZE 64

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
    printf("pager_retrieve: NUM_PAGES: %d\n", NUM_PAGES);
    for(int i = 0; i < NUM_PAGES; i++){
        printf("pager_retrieve: Checking page %d: page_nr=%d, new=%d, last_accessed=%d\n", i, pages[i].page_nr, pages[i].new, pages[i].last_accessed);
        if(pages[i].page_nr == page_nr && pages[i].new != 0){
            printf("pager_retrieve: Page %d found in memory\n", page_nr);
            return &pages[i];
        }
    }
    printf("pager_retrieve: Page %d not found in memory\n", page_nr);
    return NULL;
}

page_t *pager_get_available_page(){
    for(int i = 0; i < NUM_PAGES; i++){
        if(pages[i].pinned == 0){
            printf("pager_get_available_page: Returning unpinned/available page %d\n", i);
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


void pager_init(int fd){
    pager_read_footer(fd);

    // Simple stub for readability
    // Maybe do some more stuff here, don't know yet
}

void pager_reconstruct_schema(int fd){
    printf("Reconstructing schema\n");
    print_schema(&sch);
    int schema_start = file_size - 0x8 - footer_size;
    pager_read(fd, schema_start, &sch, sizeof(schema_t), 1);
    print_schema(&sch);
}

void pager_reconstruct_field_descriptors(int fd, int start_page, int end_page){
    printf("Reconstructing field descriptors from footer pages %d to %d\n", start_page, end_page);
    int field_desc_start = file_size - 0x8 - footer_size + sizeof(schema_t);
    field_desc_t *prev = NULL;
    for(int i = 0; i < sch.field_amount; i++){
        field_desc_t *field = malloc(sizeof(field_desc_t));
        pager_read(fd, field_desc_start + i * (sizeof(field_desc_t) - 0x8), field, sizeof(field_desc_t) - 0x8, 1);
        field->next = NULL;
        if(prev == NULL) head = field;
        else prev->next = field;
        prev = field;

        total_fields_size += field->size;
    }
    print_field_descriptors(head);
}

void pager_insert_record(int fd, record rc){
    if(sch.record_amount % sch.max_rg_record_amount == 0 && sch.record_amount != 0){
        // Make space for new row group 
        sch.record_amount++;
        pager_write_new_row_group(fd);
    }
    // Calculate row group to insert into and offset within that row group
    int available_row_group = sch.record_amount / sch.max_rg_record_amount;
    int available_row_group_offset = ((total_fields_size * sch.max_rg_record_amount) * available_row_group) + 4;
    printf("Pager_insert_record: Available row group %d from record amount %d and max rg record amount %d\n", available_row_group, sch.record_amount, sch.max_rg_record_amount);
    printf("Pager_insert_record: Total row group size %d\n", total_fields_size * sch.max_rg_record_amount);
    printf("Pager_insert_record: Available offset %d\n", available_row_group_offset);

    int i = 0;
    for(field_desc_t *cur = head; cur; cur = cur->next, i++){
        // Calculate offset for this column within the row group and write the column value to that offset
        int slot_offset = available_row_group_offset + cur->size * (sch.record_amount % sch.max_rg_record_amount);
        printf("Pager_insert_record: Writing %d bytes at %d\n", cur->size, slot_offset);
        pager_write(fd, slot_offset, rc[i], cur->size, 0);
        available_row_group_offset += cur->size * sch.max_rg_record_amount;
    }

    // Update record amount in schema and write it to disk
    sch.record_amount++;
    int schema_start = file_size - 0x8 - footer_size;
    pager_write(fd, schema_start, &sch.record_amount, sizeof(int), 1);

    // Temporary flush to see result in disk, will later be replace
    // with a more streamlined flushing approach.
    pager_flush_all(fd);
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
    pager_write(fd, new_footer_start, &sch, sizeof(schema_t), 1);
    cur_footer_size += sizeof(schema_t);

    // Write field descriptors to new location
    for(field_desc_t *cur = head; cur; cur = cur->next){
        pager_write(fd, new_footer_start + sizeof(schema_t) + (cur->ColumnID * (sizeof(field_desc_t) - 0x8)), cur, sizeof(field_desc_t) - 0x8, 1);
        cur_footer_size += sizeof(field_desc_t) - 0x8;
    }

    // Cheap solution, recalculate column offsets for old + new row groups all over again
    int num_row_groups = (sch.record_amount / sch.max_rg_record_amount) + 1;
    for (int rg = 0; rg < num_row_groups + 1; rg++) {
        int col_offset = 4 + (rg * total_row_group_size);  // 4 = magic header
        for (field_desc_t *cur = head; cur; cur = cur->next) {
            pager_write(fd, new_footer_start + cur_footer_size, &col_offset, sizeof(int), 1);
            cur_footer_size += sizeof(int);
            col_offset += cur->size * sch.max_rg_record_amount;
        }
    }

    // Write footer size to new location
    pager_write(fd, new_footer_start + cur_footer_size, &cur_footer_size, sizeof(int), 1);
    footer_size = cur_footer_size;
    file_size += total_row_group_size + (sch.field_amount * sizeof(int)); // Update file size to include new row group + column offsets + footer size
    // Write magic number to new location
    pager_write(fd, new_footer_start + cur_footer_size + sizeof(int), "SAM1", 4, 1);
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
            pg = pager_get_empty_page();
            if(pg == NULL){
                // Evict a LRU page to disk and load the requested page from disk
                printf("No available pages to load footer page %d, evict and load\n", page_num);
                pg = pager_evict_page(fd, page_num, 1);
            } else{
                // Initialize page metadata
                pg->page_nr = page_num;
                page_pin(page_num);
                pg->content = calloc(PAGE_SIZE, 1);
                pg->new = 1;
            }
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
 * i.e. passing a pointer to page->content as `dest`, like
 * pager_read(fd, 0x1000, page->content, 128, 0);
 */
void pager_read(int fd, int start_addr, void *dest, int size, int to_pin) {
    int bytes_copied = 0;

    if(size <= 0) { printf("PAGER READ ERROR: Reading size <= 0\n"); return; }
    if(dest == NULL) { printf("PAGER READ ERROR: Reading into NULL buffer\n"); return; }

    while (bytes_copied < size) {
        int current_addr = start_addr + bytes_copied;
        int page_num      = current_addr / PAGE_SIZE;
        int page_offset   = current_addr % PAGE_SIZE;

        page_t *pg = pager_retrieve(page_num);
        if(pg == NULL){
            // Will not be accessed when setting up footer pages because they are loaded beforehand
            // but needs to be potentially evicted and loaded in any other use-case
            pg = pager_get_empty_page();
            if(pg == NULL){
                // Evict a LRU page to disk and load the requested page from disk
                printf("No available pages to load page %d, evict and load\n", page_num);
                pg = pager_evict_page(fd, page_num, to_pin);
            } else{
                // Initialize new page metadata
                pg->page_nr = page_num;
                pg->pinned = to_pin;
                pg->content = calloc(PAGE_SIZE, 1);
                pg->new = 1;
            }
            // Load the evicted page's content from disk
            int bytes_read = storage_read(fd, page_num * PAGE_SIZE, pg->content, PAGE_SIZE);
            pg->valid_bytes = bytes_read;
        }

        int bytes_left_in_page = pg->valid_bytes - page_offset;
        int bytes_left_to_read = size - bytes_copied;
        int chunk = (bytes_left_in_page < bytes_left_to_read)
                        ? bytes_left_in_page
                        : bytes_left_to_read;

        memcpy((unsigned char *)dest + bytes_copied, pg->content + page_offset, chunk);

        bytes_copied += chunk;
        pg->last_accessed++;
    }
}

/*
 * Writes `size` bytes from `src` to absolute page offset `start_addr`,
 * transparently crossing as many pages as necessary. Loads any page not
 * already cached. Caller owns `src` (must be pre-allocated, size >= `size`).
 * NOTE: This function copies from `src` into pages.
 */
void pager_write(int fd, int start_addr, void *src, int size, int to_pin){
    int bytes_written = 0;

    if(size <= 0) { printf("PAGER WRITE ERROR: Writing size <= 0\n"); return; }
    if(src == NULL) { printf("PAGER WRITE ERROR: Writing NULL buffer\n"); return; }

    while (bytes_written < size) {
        int current_addr = start_addr + bytes_written;
        int page_num     = current_addr / PAGE_SIZE;
        int page_offset  = current_addr % PAGE_SIZE;

        page_t *pg = pager_retrieve(page_num);
        if(pg == NULL){
            pg = pager_get_empty_page();
            if(pg == NULL){
                // Evict a LRU page to disk and load the requested page from disk
                printf("No available pages to load page %d, evict and load\n", page_num);
                pg = pager_evict_page(fd, page_num, to_pin);
            } else{
                // Initialize new page metadata
                pg->page_nr = page_num;
                pg->pinned = to_pin;
                pg->content = calloc(PAGE_SIZE, 1);
                pg->new = 1;
            }
            // Load the evicted page's content from disk
            int bytes_read = storage_read(fd, page_num * PAGE_SIZE, pg->content, PAGE_SIZE);
            pg->valid_bytes = bytes_read;
        }

        int bytes_left_in_page = PAGE_SIZE - page_offset;
        int bytes_left_to_write = size - bytes_written;
        int chunk = (bytes_left_in_page < bytes_left_to_write)
                        ? bytes_left_in_page
                        : bytes_left_to_write;

        memcpy(pg->content + page_offset, (unsigned char *)src + bytes_written, chunk);
        pg->dirty = 1;

        bytes_written += chunk;
        pg->last_accessed++;
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
        printf("Flushing dirty page %d to disk\n", page->page_nr);
        storage_write(fd, start_addr, page->content, page->valid_bytes);
        page->dirty = 0;
    }
}

// Perhaps also load page at same time, unsure right now.
page_t *pager_evict_page(int fd, int page_nr, int to_pin){
    // Find a page to evict (must be not pinned)
    page_t *page = find_LRU();
        if(page == NULL){
        printf("pager_evict_page: all pages pinned, cannot evict\n");
        return NULL;
    }
    printf("Evicting page %d: last_accessed %d, dirty %d, pinned %d, new %d\n", page->page_nr, page->last_accessed, page->dirty, page->pinned, page->new);
    if(!page->pinned){
        if(page->dirty){
            // Flush dirty page to disk before eviction
            printf("Evicting dirty page %d, flushing to disk\n", page->page_nr);
            pager_flush(fd, page);
            page->dirty = 0;
            page->valid_bytes = 0; // Reset valid bytes since it's being evicted
        }
        page->page_nr = page_nr;
        page->pinned = to_pin;
        page->last_accessed = 0; // Reset LRU for new page
        return page;
    }

    printf("No unpinned pages available for eviction\n");
    return NULL;
}


page_t *find_LRU(){
    int LRU = -1; 

    for(int i = 0; i < NUM_PAGES; i++){
        if(pages[i].pinned != 0) continue;  // skip pinned pages entirely

        if(LRU == -1 || pages[i].last_accessed <= pages[LRU].last_accessed)
            LRU = i;
        
    }

    if(LRU == -1){
        printf("find_LRU: all pages are pinned, cannot evict\n");
        return NULL;
    }

    return &pages[LRU];
}

void pager_flush_all(int fd){
    for(int i = 0; i < NUM_PAGES; i++){
        if(pages[i].new && pages[i].dirty){
            pager_flush(fd, &pages[i]);
        }
    }
}

page_t  *pager_get_empty_page(){
    for(int i = 0; i < NUM_PAGES; i++){
        if(pages[i].new == 0){
            return &pages[i];
        }
    }
    return NULL;
}


char **pager_parse_query(char *filename, char **projected_cols, int projected_cols_count, char *filtered_col, int filtered_val, int *total_res_column_vals, char *cmp_op){
    int fd = find_file(filename);
    if(!validate_file(fd)) { printf("Wrong file type\n"); return NULL; }

    if(!sch.field_amount){
        printf("pager_parse_query: schema not allocated\n");
        pager_read_footer(fd);
    }

    if(sch.record_amount == 0){ printf("pager_parse_query: No records to query for\n"); return NULL; }

    filter_res_t *query_res = NULL;
    query_res = pager_filter(fd, cmp_op, filtered_col, filtered_val);
    pager_project(fd, query_res, projected_cols, projected_cols_count);

    // Not completely done yet
    return NULL;
}


filter_res_t *pager_filter(int fd, char *cmp_op, char *filtered_col, int filtered_val){
    cmpfunc_t cmpfunc = determine_op(cmp_op);
    if(!cmpfunc){ printf("Unsupported operator %s\n", cmp_op); return NULL; }

    // find filtered field metadata
    int filtered_field_colID = -1, filtered_field_size = 0;
    for(field_desc_t *cur = head; cur; cur = cur->next){
        if(strcmp(cur->name, filtered_col) == 0){
            filtered_field_colID = cur->ColumnID;
            filtered_field_size  = cur->size;
            break;
        }
    }
    if(filtered_field_colID == -1){ printf("Field %s not found\n", filtered_col); return NULL; }

    // allocate result
    filter_res_t *res = malloc(sizeof(filter_res_t));
    res->indices = malloc(sizeof(int) * sch.record_amount);
    res->count   = 0;

    int num_rgs = sch.record_amount / sch.max_rg_record_amount;
    int offset_start = file_size - 0x8 - footer_size + sizeof(schema_t) 
                     + (sizeof(field_desc_t) - 0x8) * sch.field_amount;

    for(int rg = 0; rg <= num_rgs; rg++){
        // how many records in this row group
        int rg_records = (rg == num_rgs)
            ? sch.record_amount % sch.max_rg_record_amount
            : sch.max_rg_record_amount;
        if(rg_records == 0) break;  // record_amount is exact multiple, no partial last rg

        // get this row group's offset for the filtered column
        // offset_start + (rg * field_amount + colID) * sizeof(int)
        int cc_offset_addr = offset_start + (rg * sch.field_amount + filtered_field_colID) * sizeof(int);
        int cc_offset;
        pager_read(fd, cc_offset_addr, &cc_offset, sizeof(int), 0);

        // read the column chunk
        int cc_buf[MAX_RG_RECORD_AMOUNT];
        pager_read(fd, cc_offset, cc_buf, filtered_field_size * rg_records, 0);

        // calculate the global row index of each filtered row
        for(int i = 0; i < rg_records; i++){
            if(cmpfunc(cc_buf[i], filtered_val)){
                res->indices[res->count++] = rg * sch.max_rg_record_amount + i;
            }
        }
    }
    return res;
}


void pager_project(int fd, filter_res_t *filter_res, char **projected_cols, int num_projected_cols){
    // Filter_res will be NULL if no filtering option was provided, i.e. do scan of all rows
    if(filter_res){
        int offset_start = file_size - 0x8 - footer_size + sizeof(schema_t)
                        + (sizeof(field_desc_t) - 0x8) * sch.field_amount;

        for(int i = 0; i < filter_res->count; i++){
            int idx       = filter_res->indices[i];
            int rg        = idx / sch.max_rg_record_amount;
            int row_in_rg = idx % sch.max_rg_record_amount;

            for(int c = 0; c < num_projected_cols; c++){
                // find field descriptor for current projected column name
                field_desc_t *col = NULL;
                for(field_desc_t *cur = head; cur; cur = cur->next)
                    if(strcmp(cur->name, projected_cols[c]) == 0){ col = cur; break; }
                if(!col){ printf("Column %s not found\n", projected_cols[c]); continue; }

                // Calculate and read offset of offset on disk
                int cc_offset_addr = offset_start  + (rg * sch.field_amount + col->ColumnID) * sizeof(int);
                int cc_offset;
                pager_read(fd, cc_offset_addr, &cc_offset, sizeof(int), 0);

                // Calculate and read each individual value
                int value_offset = cc_offset + row_in_rg * col->size;
                char buf[col->size + 1];
                memset(buf, 0, col->size + 1);
                pager_read(fd, value_offset, buf, col->size, 0);

                // convert and store/print result
                if(col->type == 0){
                    int v; memcpy(&v, buf, sizeof(int));
                    printf("%s: %d  ", col->name, v);
                } else {
                    printf("%s: %s  ", col->name, buf);
                }
            }
            printf("\n");
        }
    } else {
        // Scan all rows but only projected fields
        printf("No filtering option provided, scanning all projected column chunks\n");
    }
}
