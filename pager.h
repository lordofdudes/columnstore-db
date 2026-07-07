#ifndef PAGER_H
#define PAGER_H

/*
 * Pager — page-cache layer between storage.c and the raw file.
 *
 * When implemented, storage.c will stop calling read()/write()/lseek()
 * directly and will go through pager_read() / pager_write() instead.
 * query.c and schema.c don't need to know the pager exists at all.
 *
 * Planned design:
 *
 *   pager_read(fd, page_nr)   — return a pointer to the in-memory page,
 *                               loading from disk on a cache miss,
 *                               evicting the LRU unpinned page if full.
 *                               
 *   pager_write(fd, page_nr)  — mark the page dirty; actual disk write
 *                               happens on pager_flush() or program exit.
 *
 *   pager_flush(page)           — write dirty page back to disk.
 *
 *   pager_flush_all(fd)         — flush all dirty pages for the given file.
 * 
 *   pager_pin(page)        — prevent a page from being evicted
 *                               (needed while a record is being written).
 *
 *   pager_unpin(page)      — release the pin.
 */

#define BLOCK_SIZE 256
#define NUM_PAGES  5

typedef struct page page_t;
struct page {
    unsigned char *content;
    int   page_nr;
    int   last_accessed;
    int   dirty;
    int   pinned;
    int   new;
    int   valid_bytes;
};
extern page_t pages[NUM_PAGES];

// General pager functions
void    pager_init(int fd);
page_t *pager_get_available_page();
void    pager_write(int fd, int start_addr, void *src, int size, int to_pin);
void    pager_read(int fd, int start_addr, void *dest, int size, int to_pin);
void    pager_flush(int fd, page_t *page);
void    pager_flush_all(int fd);
void    pager_pin(page_t *page);
void    pager_unpin(page_t *page);
int     pager_insert_row(char *filename, char **col_vals);
page_t  *pager_get_empty_page();


    // So if a file has an Int (4 bytes) and PAGE_SIZE 2 and you want to calculate the page(s) that the int is in, you would do:
    // Start page = file_size - sizeof(int) / PAGE_SIZE = 4 - 4 / 2 = 0
    // End page = file_size - 1 / PAGE_SIZE = 4 - 1 / 2 = 3 / 2 = 1
    // byte[0] = 4 - 4 / 2 = 0, byte[1] = 4 - 3 / 2 = 0, byte[2] = 4 - 2 / 2 = 1, byte[3] = 4 - 1 / 2 = 1

// Footer-related functions
int  pager_read_footer(int fd);
void pager_write_new_row_group(int fd);


// LRU, eviction and loading functions
page_t *pager_evict_page(int fd, int page_nr, int to_pin);
page_t *find_LRU();

#endif