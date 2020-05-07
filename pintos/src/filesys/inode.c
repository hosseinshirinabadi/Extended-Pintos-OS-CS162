#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define NUM_DIRECT_POINTERS 12
#define NUM_POINTERS_PER_INDIRECT BLOCK_SECTOR_SIZE / sizeof(block_sector_t)

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {

    block_sector_t direct_pointers[NUM_DIRECT_POINTERS];
    block_sector_t indirect_pointer;
    block_sector_t doubly_indirect_pointer;

    bool is_dir;

    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[111];               /* Not used. */ // 125 old, 111 new
  };


struct indirect_disk {
  block_sector_t pointers[NUM_POINTERS_PER_INDIRECT];
};

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
  };


static struct inode_disk *get_inode_disk(struct inode *inode) {
  char buffer[BLOCK_SECTOR_SIZE];
  read_from_cache(inode->sector, buffer);
  struct inode_disk *result = buffer;
  return result;
}

static struct indirect_disk *get_indirect_disk(block_sector_t sector_number) {
  char buffer[BLOCK_SECTOR_SIZE];
  read_from_cache(sector_number, buffer);
  struct inode_disk *result = buffer;
  return result;
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos)
{
  ASSERT (inode != NULL);

  struct inode_disk *disk_data = get_inode_disk(inode);

  if (pos < 0) {
    return -1;
  }

  if (pos < disk_data->length) {

    // check if position is in one of the 12 direct blocks
    if (pos < NUM_DIRECT_POINTERS * BLOCK_SECTOR_SIZE) {
      int direct_index = pos / BLOCK_SECTOR_SIZE;
      block_sector_t result_sector = disk_data->direct_pointers[direct_index];
      return result_sector;

    // check if position is in one of the blocks in the indirect pointer
    } else if (pos < NUM_DIRECT_POINTERS*BLOCK_SECTOR_SIZE + NUM_POINTERS_PER_INDIRECT*BLOCK_SECTOR_SIZE) {
      block_sector_t indirect_sector = disk_data->indirect_pointer;
      struct indirect_disk *indirect_block = get_indirect_disk(indirect_sector);
      int indirect_index = (pos / BLOCK_SECTOR_SIZE) - NUM_DIRECT_POINTERS;
      block_sector_t result_sector = indirect_block->pointers[indirect_index];
      return result_sector;

    // position is in one of the blocks in the doubly indirect pointer
    } else {
      block_sector_t doubly_indirect_sector = disk_data->doubly_indirect_pointer;
      struct indirect_disk *doubly_indirect_block = get_indirect_disk(doubly_indirect_sector);

      // get the appropriate singly indirect block
      int sector_idx = pos / BLOCK_SECTOR_SIZE;
      // int doubly_indirect_index = (DIV_ROUND_UP(pos,512) - (12 + 128)) / 128;
      int doubly_indirect_index = (sector_idx - (NUM_DIRECT_POINTERS+NUM_POINTERS_PER_INDIRECT)) / NUM_POINTERS_PER_INDIRECT;
      if (doubly_indirect_index < 0) {
        doubly_indirect_index = 0;
      }

      block_sector_t indirect_sector = doubly_indirect_block->pointers[doubly_indirect_index];
      struct indirect_disk *indirect_block = get_indirect_disk(indirect_sector);

      // get the appropriate data block
      // int indirect_index = (DIV_ROUND_UP(pos,512) - (12 + 128)) % 128;
      int indirect_index = (sector_idx - (NUM_DIRECT_POINTERS + NUM_POINTERS_PER_INDIRECT));
      if (indirect_index < 0) {
        indirect_index = 0;
      }
      indirect_index = indirect_index % NUM_POINTERS_PER_INDIRECT;
      block_sector_t result_sector = indirect_block->pointers[indirect_index];

      return result_sector;
    }
  } else {
    return -1;
  }
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = true;

  ASSERT (length >= 0);


  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL) {

      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;

      if (sectors == 0) {
        free(disk_inode);
        return success;
      }

      int num = sectors;
      if (sectors > 12) {
        num = 12;
      }

      // DIRECT BLOCKS: create direct data blocks
      for (int i = 0; i < num; i++) {
        if (!free_map_allocate (1, &disk_inode->direct_pointers[i])) {
          free(disk_inode);
          return false;
        }
        char zeros[BLOCK_SECTOR_SIZE];
        memset(zeros, 0, BLOCK_SECTOR_SIZE);
        write_to_cache(disk_inode->direct_pointers[i], zeros);
      }

      // INDIRECT BLOCK: create indirect block, along with its data blocks
      struct indirect_disk *direct_pointers = malloc(sizeof(struct indirect_disk));
      memset(direct_pointers, 0, sizeof(struct indirect_disk));
      if (sectors > 12) {
        // allocate the indirect block
        if (!free_map_allocate (1, &disk_inode->indirect_pointer)) {
          free(direct_pointers);
          free(disk_inode);
          return false;
        }

        num = sectors - NUM_DIRECT_POINTERS;
        if (num > NUM_POINTERS_PER_INDIRECT) {
          num = NUM_POINTERS_PER_INDIRECT;
        }

        // create the needed data blocks in the indirect block
        for (int i = 0; i < num; i++) {
          if (!free_map_allocate (1, &direct_pointers->pointers[i])) {
            free(direct_pointers);
            free(disk_inode);
            return false;
          }
          
          char zeros[BLOCK_SECTOR_SIZE];
          memset(zeros, 0, BLOCK_SECTOR_SIZE);
          write_to_cache(direct_pointers->pointers[i], zeros);
        }

        write_to_cache(disk_inode->indirect_pointer,  direct_pointers);
      }
      free(direct_pointers);


      // DOUBLY INDIRECT BLOCK: create doubly indirect blong, along with its indirect and direct blocks
      int sectors_left = sectors - (NUM_DIRECT_POINTERS + NUM_POINTERS_PER_INDIRECT);
      if (sectors_left < 0) {
        sectors_left = 0;
      }
      int num_indirects = DIV_ROUND_UP(sectors_left, NUM_POINTERS_PER_INDIRECT);

      // indirect pointers of the doubly indirect block
      struct indirect_disk *indirect_pointers = malloc(sizeof(struct indirect_disk)); 
      memset(indirect_pointers, 0, sizeof(struct indirect_disk));
      
      // direct data pointers of the doubly indrect block
      struct indirect_disk data_pointers[num_indirects];
      if (sectors > NUM_DIRECT_POINTERS + NUM_POINTERS_PER_INDIRECT) {
        // allocate the doubly indirect block
        if (!free_map_allocate (1, &disk_inode->doubly_indirect_pointer)) {
          free(indirect_pointers);
          free(data_pointers);
          free(disk_inode);
          return false;
        }

        // create the needed indirect blocks
        for (int i = 0; i < num_indirects; i++) {
          int num_left = sectors_left;
          if (sectors_left > NUM_POINTERS_PER_INDIRECT) {
            num_left = NUM_POINTERS_PER_INDIRECT;
          }

          // allocate one indirect block
          if (!free_map_allocate (1, &indirect_pointers->pointers[i])) {
            free(indirect_pointers);
            free(data_pointers);
            free(disk_inode);
            return false;
          }


          // create the data blocks for this indirect block
          for (int j = 0; j < num_left; j++) {
            if (!free_map_allocate (1, &(data_pointers[i].pointers[j]))) {
              free(indirect_pointers);
              free(data_pointers);
              free(disk_inode);
              return false;
            }
            char zeros[BLOCK_SECTOR_SIZE];
            memset(zeros, 0, BLOCK_SECTOR_SIZE);
            write_to_cache(data_pointers[i].pointers[j], zeros);
            sectors_left--;
          }

          write_to_cache(indirect_pointers->pointers[i],  &data_pointers[i]);
        }

        write_to_cache(disk_inode->doubly_indirect_pointer, indirect_pointers);   
      }
      free(indirect_pointers);


      write_to_cache(sector, disk_inode);
      free(disk_inode);
    }

  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector)
        {
          inode_reopen (inode);
          return inode;
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode)
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0) {

      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      /* Deallocate blocks if removed. */
      if (inode->removed) {

        struct inode_disk *disk_data = get_inode_disk(inode);

        // number of sectors allocated for this inode
        int sectors = bytes_to_sectors(disk_data->length);
        if (sectors == 0) {
          free_map_release (inode->sector, 1);
        } else {


          // Releasing direct blocks
          for (int i = 0; i < NUM_DIRECT_POINTERS; i++) {
            free_map_release(disk_data->direct_pointers[i], 1);
            sectors--;
            if (sectors == 0) {
              break;
            }
          }

          // Releasing indirect block along with its data blocks
          struct indirect_disk* indirect_block; 
          if (sectors > 0) {
            indirect_block = get_indirect_disk(disk_data->indirect_pointer);
            for (int i = 0; i < NUM_POINTERS_PER_INDIRECT; i++) {
              free_map_release (indirect_block->pointers[i], 1);
              sectors--;
              if (sectors == 0) {
                break;
              }
            }
            free_map_release (disk_data->indirect_pointer, 1);
          }

          // Releasing doubly indirect pointer, along with its indirect and direct blocks
          struct indirect_disk* doubly_indirect_block; 
          if (sectors > 0) {
            doubly_indirect_block = get_indirect_disk(disk_data->doubly_indirect_pointer);
            int j = 0;
            while (sectors > 0) {
              indirect_block = get_indirect_disk(doubly_indirect_block->pointers[j]);
              // release the data blocks for this indirect block
              for (int i = 0; i < NUM_POINTERS_PER_INDIRECT; i++) {
                free_map_release (indirect_block->pointers[i], 1);
                sectors--;
                if (sectors == 0) {
                  break;
                }
              }
              // release one indirect block
              free_map_release (disk_data->indirect_pointer, 1);
              j++;
            }
            // release the doubly indirect block
            free_map_release (disk_data->doubly_indirect_pointer, 1);
          }

          // release the inode itself
          free_map_release (inode->sector, 1);     
        }          
      }
      free (inode);
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  char buff[BLOCK_SECTOR_SIZE];
  read_from_cache(inode->sector, buff);
  struct inode_disk *disk_data = buff;

  if (disk_data->length < offset) {
    return 0;
  }

  if (disk_data->length < size + offset) {
    size = disk_data->length;
  }

  while (size > 0)
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          read_from_cache(sector_idx, buffer + bytes_read);
        }
      else
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL)
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          read_from_cache(sector_idx, bounce);
          memcpy(buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{
  const uint8_t *buffer = buffer_;
  off_t OGoffset = offset;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt) {
    return 0;
  }

  struct inode_disk *disk_data = get_inode_disk(inode);

  int file_length = disk_data->length;
  int allocated_sectors = bytes_to_sectors(file_length);
  int needed_sectors = bytes_to_sectors(size + offset) - allocated_sectors;

  static char zeros[BLOCK_SECTOR_SIZE];
  memset(zeros, 0, BLOCK_SECTOR_SIZE);

  if (size + offset > file_length) {
    // expand

    // DIRECT POINTERS SECTION:
    int num = needed_sectors;
    if (allocated_sectors <= NUM_DIRECT_POINTERS) {
      if (num > NUM_DIRECT_POINTERS - allocated_sectors) {
        num = NUM_DIRECT_POINTERS - allocated_sectors;
      }
      // allocate needed direct pointers
      for (int i = allocated_sectors; i < allocated_sectors + num; i++) {
        if (!free_map_allocate (1, &disk_data->direct_pointers[i])) {
          return 0;
          //goto write_data;
        }
        write_to_cache(disk_data->direct_pointers[i], zeros);
        needed_sectors--;
      }
    }

    // INDIRECT POINTER SECTION:
    if (needed_sectors > 0 && allocated_sectors <= (NUM_DIRECT_POINTERS + NUM_POINTERS_PER_INDIRECT)) {
      int current_pos = allocated_sectors - NUM_DIRECT_POINTERS;
      if (current_pos < 0) {
        current_pos = 0;
      }

      struct indirect_disk *indirect_block;
      if (current_pos == 0) {
        // need to create the indirect block
        indirect_block = malloc(sizeof(struct indirect_disk)); 
        if (!free_map_allocate (1, &disk_data->indirect_pointer)) {
          return 0;
          //goto write_data;
        }
      } else {
        // indirect block already exists
        indirect_block = get_indirect_disk(disk_data->indirect_pointer);
      }

      num = needed_sectors;
      if (num > NUM_POINTERS_PER_INDIRECT - current_pos) {
        num = NUM_POINTERS_PER_INDIRECT - current_pos;
      }

      // create the needed data blocks
      for (int i = current_pos; i < current_pos + num; i++) {
        if (!free_map_allocate (1, indirect_block->pointers + i)) {
          return 0;
          //goto write_data;
        }
        write_to_cache(indirect_block->pointers[i], zeros);
        needed_sectors--;
      }

      write_to_cache(disk_data->indirect_pointer, indirect_block);
    }

    // DOUBLY INDRECT POINTER SECTION:
    if (needed_sectors > 0) {
      int current_pos = allocated_sectors - (NUM_DIRECT_POINTERS + NUM_POINTERS_PER_INDIRECT);
      if (current_pos < 0) {
        current_pos = 0;
      }

      struct indirect_disk *doubly_indirect_block;
      if (current_pos == 0) {
        // need to allocated the doubly indirect block
        doubly_indirect_block = malloc(sizeof(struct indirect_disk));
        if (!free_map_allocate (1, &disk_data->doubly_indirect_pointer)) {
          return 0;
          //goto write_data;
        }
      } else {
        // doubly indirect block already exists
        doubly_indirect_block = get_indirect_disk(disk_data->doubly_indirect_pointer);
      }

      // int used_indirect_blocks = DIV_ROUND_UP(current_pos, NUM_POINTERS_PER_INDIRECT);
      int used_indirect_blocks = current_pos / NUM_POINTERS_PER_INDIRECT;
      int indirect_offset = current_pos % NUM_POINTERS_PER_INDIRECT;

      // finish off the rest of the used indirect block
      struct indirect_disk *indirect_block;
      if (indirect_offset != 0) {
      
        indirect_block = get_indirect_disk(doubly_indirect_block->pointers[used_indirect_blocks]);

        num = needed_sectors;
        if (num > NUM_POINTERS_PER_INDIRECT - indirect_offset) {
          num = NUM_POINTERS_PER_INDIRECT - indirect_offset;
        }

        for (int i = indirect_offset; i < indirect_offset + num; i++) {
          if (!free_map_allocate (1, &indirect_block->pointers[i])) {
            return 0;
          //goto write_data;
          }
          write_to_cache(indirect_block->pointers[i], zeros);
          needed_sectors--;
        }

        write_to_cache(doubly_indirect_block->pointers[used_indirect_blocks], indirect_block);
        used_indirect_blocks++;
      }

      // fill up the rest of the indirect blocks
      while (needed_sectors > 0) {
        indirect_block = malloc(sizeof(struct indirect_disk));
        if (!free_map_allocate (1, &doubly_indirect_block->pointers[used_indirect_blocks])) {
          return 0;
          //goto write_data;
        }

        num = needed_sectors;
        if (num > NUM_POINTERS_PER_INDIRECT) {
          num = NUM_POINTERS_PER_INDIRECT;
        }

        // create the data blocks for this indirect block
        for (int i = 0; i < num; i++) {
          if (!free_map_allocate (1, &indirect_block->pointers[i])) {
            return 0;
          //goto write_data;
          }
          write_to_cache(indirect_block->pointers[i], zeros);
          needed_sectors--;
        }

        write_to_cache(doubly_indirect_block->pointers[used_indirect_blocks], indirect_block);
        used_indirect_blocks++;
        free(indirect_block);
      }

      write_to_cache(disk_data->doubly_indirect_pointer, doubly_indirect_block);
    }

    disk_data->length = offset + size;
    write_to_cache(inode->sector, disk_data);
  }

  //write_data:
  while (size > 0) {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

       if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE) {
          /* Write full sector directly to disk. */
        write_to_cache(sector_idx, buffer + bytes_written);
      } else {
        /* We need a bounce buffer. */
        if (bounce == NULL)
          {
            bounce = malloc (BLOCK_SECTOR_SIZE);
            if (bounce == NULL)
              break;
          }

        /* If the sector contains data before or after the chunk
           we're writing, then we need to read in the sector
           first.  Otherwise we start with a sector of all zeros. */
        if (sector_ofs > 0 || chunk_size < sector_left)
          read_from_cache(sector_idx, bounce);
        else
          memset (bounce, 0, BLOCK_SECTOR_SIZE);
        memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
        write_to_cache(sector_idx, bounce);
      }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;

    }
  free (bounce);

  // if (OGoffset + bytes_written > disk_data->length) {
  //   disk_data->length = OGoffset + bytes_written;
  // }

  // if (offset + size > disk_data->length) {
  //   disk_data->length = (size + offset);
  // }

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  // return inode->data.length;
  struct inode_disk *disk_inode = get_inode_disk(inode);
  return disk_inode->length;
}
