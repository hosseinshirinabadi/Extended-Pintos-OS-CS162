#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"


struct cache_block;

void initialize_cache(void);
void read_from_cache(block_sector_t target_sector, void *buff);
void write_to_cache(block_sector_t target_sector, void *buff);
void flush_cache(void);

#endif /* filesys/cache.h */
