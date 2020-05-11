#include <random.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"
#include "filesys/cache.h"
#include "threads/malloc.h"

static char tempBuf[1];
void
test_main (void)
{
  int file;
  size_t num_bytes;
  int index = 0;
  int init_read;
  int init_write;
  int num_reads;
  int num_writes;
  int num_write_bytes = 65536;   /* for 64KB written byte-by-byte */


  CHECK (create ("xyz", 0), "create \"xyz\"");
  CHECK ((file = open ("xyz")) > 1, "open \"xyz\" for writing");

  init_read = get_cache (3);
  init_write = get_cache (4);

  msg ("writing 64kB to \"xyz\" byte-by-byte");
  
  while (index < num_write_bytes) {
    random_init (0);
    random_bytes (tempBuf, sizeof tempBuf);
    num_bytes = write (file, tempBuf, 1);
    if (num_bytes != 1)
      fail ("didn't write proper number of bytes");
    index ++;
  }
  index = 0;

  msg("close \"xyz\" after writing");
  close(file);

  CHECK ((file = open ("xyz")) > 1, "open \"xyz\" for read");
  while (index < num_write_bytes) {
    num_bytes = read (file, tempBuf, 1);
    if (num_bytes != 1)
      fail ("didn't read proper number of bytes");
    index ++;
  }

  num_reads = get_cache (3)-init_read;
  num_writes = get_cache (4)-init_write;

  if (num_writes < 138 && num_writes > 118) 
    msg("total number of device writes is on the order of 128");
  
  close (file);
  remove("a");

}