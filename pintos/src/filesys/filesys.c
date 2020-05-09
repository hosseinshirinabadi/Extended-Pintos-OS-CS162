#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format)
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format)
    do_format ();

  free_map_open ();

  struct dir* dir = dir_open_root();

  thread_current()->current_directory = dir;

  

}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void)
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size)
{
  block_sector_t inode_sector = 0;
  struct dir *dir = dir_open_root();
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, name, inode_sector));
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}



bool
filesys_create_file (const char *name, off_t initial_size)
{
  block_sector_t inode_sector = 0;
  struct resolve_metadata *metadata = resolve_path(thread_current()->current_directory, name, true);
  if (!metadata) {
    return false;
  }
  struct dir *dir = get_parent_dir(metadata);
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, get_last_filename(metadata), inode_sector));
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}
 
block_sector_t
filesys_create_dir (const char *name, off_t initial_size, struct dir* pDir)
{
  block_sector_t inode_sector = 0;
  struct dir *dir = pDir;
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && dir_create (inode_sector, initial_size)
                  && dir_add (dir, name, inode_sector));


// mkdir /0
// mkdir /0/0
// mkdir /0/0/0
// if(success) {
//   struct inode_disk* disk;
//   char buffer[BLOCK_SECTOR_SIZE];
//   disk = malloc(sizeof(disk));
//   read_from_cache(inode_sector ,disk);
//   set_is_dir(disk, true);
//   write_to_cache(inode_sector, disk);
// }

  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);

  // dir_close (dir);

  // if (success) {
  //   dir_close (dir);
  // }

  return inode_sector;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, name, &inode);
  dir_close (dir);

  return file_open (inode);
}


/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open_anyPath (const char *name, struct dir * pDir)
{
  struct inode *inode = NULL;

  if (pDir != NULL)
    dir_lookup (pDir, name, &inode);
  

  return dir_open(inode);
}


struct file *filesys_open_file (const char *name, struct dir *parent_dir) { 

  struct inode *inode = NULL;

  if (parent_dir != NULL)
    dir_lookup (parent_dir, name, &inode);
  dir_close (parent_dir);

  return file_open (inode);
}


/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name)
{
  struct dir *dir = dir_open_root ();
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir);

  return success;
}

bool
filesys_remove_anyPath (const char *name, struct dir *parent_dir)
{
  // struct dir *dir = dir_open_root ();
  bool success = parent_dir != NULL && dir_remove (parent_dir, name);
  dir_close (parent_dir);

  return success;
}


/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
