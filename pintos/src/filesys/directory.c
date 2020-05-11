#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"

/* A directory. */
struct dir
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, size_t entry_cnt)
{
  //return inode_create (sector, entry_cnt * sizeof (struct dir_entry));
  bool check = inode_create (sector, entry_cnt * sizeof (struct dir_entry));
  if (check) {
    struct inode *inode_opened = inode_open(sector);
    if (inode_opened) {
      struct inode_disk *disk_data = get_inode_disk(inode_opened);
      set_is_dir(disk_data, true);
      write_to_cache(sector, disk_data);
    } else {
      return false;
    }
  }
  return check;
}




/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode)
{ 

  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      // if (inode_get_inumber(inode) == 1) {
      //   struct inode_disk *disk_data = get_inode_disk(inode);
      //   set_is_dir(disk_data, true);
      //   write_to_cache(ROOT_DIR_SECTOR, disk_data);
      // }
      dir->inode = inode;
      dir->pos = 0;

      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL;
  }

  return NULL;
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{

  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir)
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir)
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir)
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp)
{
  struct dir_entry e;
  size_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (e.in_use && !strcmp (name, e.name))
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode)
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.

     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name)
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;


  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e)
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;


 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e)
    {
      dir->pos += sizeof e;
      if (e.in_use && (strcmp(e.name, ".") != 0 && strcmp(e.name, "..") != 0))
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        }
    }
  return false;
}


/* Extracts a file name part from *SRCP into PART, and updates *SRCP so that the
next call will return the next file name part. Returns 1 if successful, 0 at
end of string, -1 for a too-long file name part. */
static int get_next_part (char part[NAME_MAX + 1], const char **srcp) {
  const char *src = *srcp;
  char *dst = part;
  /* Skip leading slashes. If it’s all slashes, we’re done. */
  while (*src == '/')
    src++;
  if (*src == '\0')
    return 0;
  /* Copy up to NAME_MAX character from SRC to DST. Add null terminator. */
  while (*src != '/' && *src != '\0') {
    if (dst < part + NAME_MAX)
      *dst++ = *src;
    else
      return -1;
    src++;
  }
  *dst = '\0';
  /* Advance source pointer. */
  *srcp = src;
  return 1;
}

struct resolve_metadata {
  struct dir *parent_dir;
  struct inode *last_inode;
  char last_file_name[NAME_MAX + 1];
};

struct dir *get_parent_dir (struct resolve_metadata *metadata) {
  return metadata->parent_dir;
}

struct inode *get_last_inode (struct resolve_metadata *metadata) {
  return metadata->last_inode;
}

char *get_last_filename (struct resolve_metadata *metadata) {
  return metadata->last_file_name;
}


struct resolve_metadata *resolve_path(struct dir *dir, char *path, bool is_mkdir) {
  char next_part[NAME_MAX + 1] = {0};

  if (dir == NULL) {
    return NULL;
  }
  if(strcmp(path, "") == 0) {
    return NULL;
  }
  struct resolve_metadata *result_metadata = malloc(sizeof(struct resolve_metadata));


  struct dir *parent_dir;
  char file_name[NAME_MAX + 1] = {0};
  struct inode *inode;
  bool inode_exists = false;
  bool isRoot = false;

  if (path[0] == '/') {
    parent_dir = dir_open_root();
    isRoot = true;
    inode = parent_dir->inode;
    char rootName[1] = "/";
    strlcpy(file_name, rootName, 2);
  } else  {      
    parent_dir = dir_reopen(dir);
    inode_exists = dir_lookup(parent_dir, ".", &inode);
    if (!inode_exists) {
      return NULL;
    }    
  }

  if (!parent_dir) {
    return NULL;
  }

  int status = get_next_part(next_part, &path);
  
  while (status > 0) {
    inode_exists = dir_lookup(parent_dir, next_part, &inode);

    // take care of mkdir traversal
    if (is_mkdir) {
      if (!inode_exists) {
        status = get_next_part(file_name, &path);
        if (status == 0) {
          result_metadata->parent_dir = parent_dir;
          result_metadata->last_inode = NULL;
          memset(result_metadata->last_file_name, 0, NAME_MAX + 1);
          strlcpy(result_metadata->last_file_name, next_part, sizeof(next_part) + 1);
          return result_metadata;
        } else {
          return NULL;
        }
      } else {
        if (path[0] == '\0') {
          return NULL;
        }
      }
    }

    if (!inode_exists) {
      return NULL;
    }
    

    if (inode_is_dir(get_inode_disk(inode)) || isRoot) {

      if (path[0] != '\0') {
        dir_close(parent_dir);
        parent_dir = dir_open(inode);
      }
      
      strlcpy(file_name, next_part, sizeof(next_part)  + 1);
      status = get_next_part(next_part, &path);
      isRoot = false;
    } else {

      char temp[NAME_MAX + 1] = {0};
      status = get_next_part(temp, &path);
      if (status != 0) {
        return NULL;
      }
      strlcpy(file_name, next_part, sizeof(next_part) + 1);
    }
  }

  if (status != 0) {
    return NULL;
  }

  result_metadata->parent_dir = parent_dir;
  result_metadata->last_inode = inode;
  memset(result_metadata->last_file_name, 0, NAME_MAX + 1);
  strlcpy(result_metadata->last_file_name, file_name, sizeof(file_name) + 1);
  return result_metadata;
}


void setup_dots_dir(block_sector_t sector, struct dir *parent_dir) {
  struct dir *child = dir_open(inode_open(sector));
  dir_add(child, ".", sector);
  dir_add(child, "..", inode_get_inumber(dir_get_inode(parent_dir)));
}
