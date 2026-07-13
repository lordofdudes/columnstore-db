#ifndef PAGER_H
#define PAGER_H


// Pager — page-cache layer between query.c (query processor) and storage.c (raw disk file).
// Any interaction / querying MUST be accessed via the page-layer, this includes reading and writing. 

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

typedef struct filter_result filter_res_t;
struct filter_result{
    int *indices;
    int count;
};

// Functions that interact directly with pages and page-metadata.
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


// Query-related functions

/**
 * @brief Performs entire query, combining both filtering and projection (if specified). 
 *  
 * @param filename              Name of file to query from
 * @param projected_cols        Names of all fields that are projected (i.e. SELECT age, name, ...)
 * @param projected_cols_count  Total amount of projected cols         ( 2 for example above)
 * @param filtered_col          Field to filter after                  (i.e. WHERE age <= 50 )
 * @param filtered_val          Value to compare chosen filter column  ( 50 for example above )
 * @param total_res_column_vals Total amount of values (8 for as example: 30, phil, 50, john, 29, sarah, 12, mark)
 * @param cmp_op                Operator used during filtering         ( <= for above WHERE statement )
 * @return char**               Contains all values after query is processed, use total_res_column_vals in unison
 */
char **pager_parse_query(char *filename, char **projected_cols, int projected_cols_count, 
           char *filtered_col, int filtered_val, int *total_res_column_vals, char *cmp_op);

/**
 * @brief Projects based on selected columns for all chosen/relevant column values
 * 
 * @param fd File to read from
 * @param filter_res Filtering results that tell index of and how many rows to project, if NULL do full scan
 * @param proj_cols  Contains name of all projected columns 
 * @param num_cols   Amount of column to project
 */
void pager_project(int fd, filter_res_t *filter_res, char **projected_cols, int num_projected_cols);

/**
 * @brief  Filters all values in selected column's chunks based on filtered_col cmp_op filtered_val
 *         
 * @param fd File to read from
 * @param cmp_op Operator used when determining which row is filtered
 * @param filtered_col Column to filter by
 * @param filtered_val Value to compare chosen column with
 * @return filter_res_t* Contains info about how many and which global row indexes passed filtering
 */
filter_res_t *pager_filter(int fd, char *cmp_op, char *filtered_col, int filtered_val);


// LRU, eviction and loading functions
page_t *pager_evict_page(int fd, int page_nr, int to_pin);
page_t *find_LRU();

#endif