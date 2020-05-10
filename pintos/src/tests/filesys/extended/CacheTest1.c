#include <random.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"
#include "filesys/cache.h"

#define NUM_ENTRIES 30
static char tempBuf[BLOCK_SECTOR_SIZE];

void
test_main (void)
{

  int file;
  size_t num_bytes;

  /* create a second file and opening it for wrtitin the buffer in it*/
  CHECK (create ("xyz", 0), "create \"xyz\"");
  CHECK ((file = open ("xyz")) > 1, "open \"xyz\" for writing");

  /* use a for loop to write random bytes into file*/
  for(int i=0; i < NUM_ENTRIES; i++) {
    /* writing random bytes into temp buffer */
    random_init (0);
    random_bytes (tempBuf, sizeof tempBuf);
    num_bytes = write (file, tempBuf, BLOCK_SECTOR_SIZE);
    if (num_bytes != BLOCK_SECTOR_SIZE)
      fail("was supposed to write %zu bytes in \"xyz\" but return value is %zu", BLOCK_SECTOR_SIZE, num_bytes);
  }

  msg ("close \"xyz\" after writing");
  close (file);

  msg("cache reset");
  /* a syscall to reset the cache */
  reset_cache();

  CHECK ((file = open ("xyz")) > 1, "open \"xyz\" for first read");
  msg ("read for the first time");
  for (int i = 0; i < NUM_ENTRIES; i++) {
    num_bytes = read (file, tempBuf, BLOCK_SECTOR_SIZE);
    if (num_bytes != BLOCK_SECTOR_SIZE)
      fail ("was supposed to read %zu bytes from \"xyz\" but return value is %zu", BLOCK_SECTOR_SIZE, num_bytes);
  }

  msg("close \"xyz\" after first read");
  close (file);
  /* calculating the hit rate of first read */
  int first_hit = cache_hit;
  int first_total = cache_hit + cache_miss;

  CHECK ((file = open ("xyz")) > 1, "open \"xyz\" for second read");
  msg ("read for the second time");
  for (int i = 0; i < NUM_ENTRIES; i ++) {
    num_bytes = read (file, tempBuf, BLOCK_SECTOR_SIZE);
    if (num_bytes != BLOCK_SECTOR_SIZE)
      fail ("was supposed to read %zu bytes from \"xyz\" but return value is %zu", BLOCK_SECTOR_SIZE, num_bytes);  
  }

  close (file);
  msg("close \"xyz\" after second read");

  // int second_hit = cache_hit;
  // int second_total = cache_hit + cache_miss;

  int first_hit_rate = (100 * first_hit) / first_total;
  int second_hit_rate = (100 * (cache_hit - first_hit)) / (cache_hit + cache_miss - first_total);

  remove ("xyz");

  if (second_hit_rate>first_hit_rate){
    msg("Second read has higher hit rate than the first time");
  }
}