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
 *   pager_flush(fd)           — write all dirty pages back to disk.
 *
 *   pager_pin(page_nr)        — prevent a page from being evicted
 *                               (needed while a record is being written).
 *
 *   pager_unpin(page_nr)      — release the pin.
 */

#include "storage.h"   /* for page_t, BLOCK_SIZE, NUM_PAGES */

/* Placeholder prototypes — fill these in when you implement the pager. */
page_t *pager_read(int fd, int page_nr);
void    pager_write(int fd, int page_nr);
void    pager_flush(int fd);
void    pager_pin(int page_nr);
void    pager_unpin(int page_nr);

#endif