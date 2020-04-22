#include "filesys/cache.h"
#include <string.h>
#include "threads/malloc.h"
#include <stdbool.h>
#include <list.h>

struct cache_block {
  block_sector_t sector;  // sector number on disk
  char data[BLOCK_SECTOR_SIZE];  // cached data
  bool dirty;  // block was written or not 
  bool valid;
  struct lock cache_lock;  // lock for this cache
  int num_readers;
  int num_writers;
  struct list_elem elem;  // list_elem used for the LRU list
};

struct cache_block cache[64];
struct list LRU;
struct lock LRU_lock;


void init_cache() { 
  // initialize cache locks
  for (int i = 0; i < 64; i++) {
    lock_init(&(cache[i].cache_lock));
  }

  // initialize LRU list and lock
  list_init(LRU);
  lock_init(LRU_lock);
}


void read_from_cache(block_sector_t target_sector, void *buff) {
  struct cache_block *block = &cache[target_sector % 64];
  if (block->sector == target_sector) {
      lock_acquire(&block->cache_lock);
      memcpy(buff, block->data, BLOCK_SECTOR_SIZE);
      lock_release (&block->cache_lock);

      lock_acquire(&LRU_lock);
      list_remove(&block->elem);
      list_push_back(&LRU, &block->elem);
      lock_release(&LRU_lock);
      return;
  } else {
    block->sector = target_sector;
    
  }

    
}


void write_to_cache(block_sector_t target_sector, void *buff) {

}



