#include <random.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"
#include "filesys/cache.h"

#define NUM_CACHE_ENTRIES 64
static char tempBuf[BLOCK_SECTOR_SIZE];

void
test_main (void)
{
  int file;
  size_t ret_val;
  random_init (0);
  random_bytes (tempBuf, sizeof tempBuf);
  CHECK (create ("a", 0), "create \"a\"");
  CHECK ((file = open ("a")) > 1, "open \"a\"");

  
  for(int i=0; i < NUM_CACHE_ENTRIES; i++)
  {
    ret_val = write (file, tempBuf, BLOCK_SECTOR_SIZE);
    if (ret_val != BLOCK_SECTOR_SIZE)
      fail ("write %zu bytes in \"a\" returned %zu",
            BLOCK_SECTOR_SIZE, ret_val);
  }
  msg ("close \"after writing\"");
  close (file);

  msg("cache reset");
  // buffer_reset ();
  reset_cache();

  CHECK ((file = open ("a")) > 1, "open \"a\"");
  msg ("read first time");
  for (int i = 0; i < NUM_CACHE_ENTRIES; i++)
  {
    ret_val = read (file, tempBuf, BLOCK_SECTOR_SIZE);
    if (ret_val != BLOCK_SECTOR_SIZE)
        fail ("read %zu bytes in \"a\" returned %zu",
              BLOCK_SECTOR_SIZE, ret_val);
  }
  close (file);
  msg("close \"a\"");

  int first_hit = cache_hit;
  int first_total = cache_hit + cache_miss;
  int first_hit_rate = (100 * first_hit) / first_total;

  CHECK ((file = open ("a")) > 1, "open \"a\"");
  msg ("read \"a\"");
  for (int i = 0; i < NUM_CACHE_ENTRIES; i ++)
  {
    ret_val = read (file, tempBuf, BLOCK_SECTOR_SIZE);
    if (ret_val != BLOCK_SECTOR_SIZE)
        fail ("read %zu bytes in \"a\" returned %zu",
              BLOCK_SECTOR_SIZE, ret_val);
  }
  close (file);
  msg("close \"a\"");

  remove ("a");


  int new_hit = cache_hit ;
  int new_total = cache_hit + cache_miss;
  int new_hit_rate = 100 * (new_hit - first_hit) / (new_total - first_total);

  if (new_hit_rate>first_hit_rate){
    msg("Second reading has higher hit rate than the first time");
  }
}