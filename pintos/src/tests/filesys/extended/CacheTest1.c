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
  int index = 0;

  /* create a second file and opening it for wrtitin the buffer in it*/
  CHECK (create ("xyz", 0), "create \"xyz\"");
  CHECK ((file = open ("xyz")) > 1, "open \"xyz\" for writing");

  /* use a for loop to write random bytes into file*/
  while (index < NUM_ENTRIES) {
    /* writing random bytes into temp buffer */
    random_init (0);
    random_bytes (tempBuf, sizeof tempBuf);
    num_bytes = write (file, tempBuf, BLOCK_SECTOR_SIZE);
    if (num_bytes != BLOCK_SECTOR_SIZE)
      fail ("didn't write proper number of bytes");
    index++;
  }
  index = 0;

  msg ("close \"xyz\" after writing");
  close (file);

  msg("cache reset");
  /* a syscall to reset the cache */
  get_cache(0);

  CHECK ((file = open ("xyz")) > 1, "open \"xyz\" for first read");
  while (index < NUM_ENTRIES) {
    num_bytes = read (file, tempBuf, BLOCK_SECTOR_SIZE);
    if (num_bytes != BLOCK_SECTOR_SIZE)
      fail ("didn't read proper number of bytes");
    index++;
  }
  index = 0;

  msg("close \"xyz\" after first read");
  close (file);
  /* calculating the hit rate of first read by using get_cache(i) syscall */
  int first_hit = get_cache(1);
  int first_total = get_cache(2);

  CHECK ((file = open ("xyz")) > 1, "open \"xyz\" for second read");
  while (index < NUM_ENTRIES) {
    num_bytes = read (file, tempBuf, BLOCK_SECTOR_SIZE);
    if (num_bytes != BLOCK_SECTOR_SIZE)
      fail ("didn't read proper number of bytes");
    index++;
  }

  close (file);
  msg("close \"xyz\" after second read");


  int first_hit_rate = first_hit / first_total;
  int second_hit_rate = (get_cache(1) - first_hit) / (get_cache(2) - first_total);

  remove ("xyz");

  if (second_hit_rate>first_hit_rate){
    msg("Second read has higher hit rate than the first time");
  }
}