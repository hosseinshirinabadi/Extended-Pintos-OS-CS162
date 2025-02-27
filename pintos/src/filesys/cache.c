#include <string.h>
#include "threads/malloc.h"
#include <stdbool.h>
#include <list.h>
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/cache.h"

struct cache_entry {
  block_sector_t sector;  // sector number on disk
  char data[BLOCK_SECTOR_SIZE];  // cached data
  bool dirty;  // block was written or not
  struct lock cache_lock;  // lock for this cache
  struct list_elem elem;  // list_elem used for the LRU list
};

struct cache_entry cache[64];
struct list LRU;
struct lock global_cache_lock; 
int counter = 0;  // keeps track of number of blocks in the cache


void initialize_cache() {
  // initialize cache locks
  for (int i = 0; i < 64; i++) {
    lock_init(&(cache[i].cache_lock));
  }

  // initialize LRU list and global lock
  list_init(&LRU);
  lock_init(&global_cache_lock);

  // for calculating hit and miss rate
  cache_access = 0;
  cache_hit = 0;
}

void read_from_cache(block_sector_t target_sector, void *buff) {
  lock_acquire(&global_cache_lock);
  cache_access++;
  struct cache_entry *block;

  int i = 0;
  while (i < counter) {
    block = &cache[i];
    lock_acquire(&block->cache_lock);
    if (block->sector == target_sector) {
      update_LRU2(block);
      lock_release(&global_cache_lock);

      memcpy(buff, block->data, BLOCK_SECTOR_SIZE);
      lock_release(&block->cache_lock);

      cache_hit++;

      return;
    }
    i++;
    lock_release(&block->cache_lock);
  }

  if (counter < 64) {
    // find an empty block
    block = &cache[counter];
    counter++;

    update_LRU1(block);
    lock_release(&global_cache_lock);

    lock_acquire(&block->cache_lock);
    block->sector = target_sector;
    block_read(fs_device, target_sector, buff);
    memcpy(block->data, buff, BLOCK_SECTOR_SIZE);
    block->dirty = false;
    lock_release(&block->cache_lock);

  } else {
    // evict and replace
    struct list_elem *block_elem = list_front(&LRU);
    block = list_entry(block_elem, struct cache_entry, elem);

    update_LRU2(block);
    lock_release(&global_cache_lock);

    lock_acquire(&block->cache_lock);
    if (block->dirty) {
      // write back to disk
      block_write(fs_device, block->sector, block->data);
    }
    block->sector = target_sector;
    block_read(fs_device, target_sector, buff);
    memcpy(block->data, buff, BLOCK_SECTOR_SIZE);
    block->dirty = false;
    lock_release(&block->cache_lock);
  } 
}

void write_to_cache(block_sector_t target_sector, void *buff) {
  lock_acquire(&global_cache_lock);
  cache_access++;
  struct cache_entry *block;

  int i = 0;
  while (i < counter) {
    block = &cache[i];
    lock_acquire(&block->cache_lock);
    if (block->sector == target_sector) {
      update_LRU2(block);
      lock_release(&global_cache_lock);

      memcpy(block->data, buff, BLOCK_SECTOR_SIZE);
      block->dirty = true;
      lock_release(&block->cache_lock);

      cache_hit++;
      
      return;
    }
    i++;
    lock_release(&block->cache_lock);
  }

  if (counter < 64) {
    // find an empty block
    block = &cache[counter];
    counter++;

    update_LRU1(block);
    lock_release(&global_cache_lock);

    lock_acquire(&block->cache_lock);
    block->sector = target_sector;
    memcpy(block->data, buff, BLOCK_SECTOR_SIZE);
    block->dirty = true;
    lock_release(&block->cache_lock);

  } else {
    // evict and replace
    struct list_elem *block_elem = list_front(&LRU);
    block = list_entry(block_elem, struct cache_entry, elem);

    update_LRU2(block);
    lock_release(&global_cache_lock);

    lock_acquire(&block->cache_lock);
    if (block->dirty) {
      // write back to disk
      block_write(fs_device, block->sector, block->data);
    }
    block->sector = target_sector;
    memcpy(block->data, buff, BLOCK_SECTOR_SIZE);
    block->dirty = true;
    lock_release(&block->cache_lock);
  }
}

void flush_cache() {
  lock_acquire(&global_cache_lock);
  struct cache_entry *block;
  for (int i = 0; i < counter; i++) {
    block = &cache[i];
    if (block->dirty == true) {
      lock_acquire(&block->cache_lock);
      block->dirty = false;
      block_write(fs_device, block->sector, block->data);
      lock_release(&block->cache_lock);
    }
  }
  lock_release(&global_cache_lock);
}

void update_LRU1(struct cache_entry *block) {
  list_push_back(&LRU, &block->elem);
}

void update_LRU2(struct cache_entry *block) {
  list_remove(&block->elem);
  list_push_back(&LRU, &block->elem);
}


bool reset_cache() {

  // flush cache to disk so old data is not lost
  flush_cache();

  lock_acquire(&global_cache_lock);

  int i = 0;
  while (i < counter) {
    struct cache_entry *block = &cache[i];
    block->sector = 0;
    i++;
  }

  counter = 0;
  list_init(&LRU);

  cache_hit = 0;
  cache_access = 0;

  lock_release(&global_cache_lock);

  return true;
}
