#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/cache.h"
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format)
{
  filesys_disk = disk_get (0, 1);
  if (filesys_disk == NULL)
    PANIC ("hd0:1 (hdb) not present, file system initialization failed");

  inode_init ();
  free_map_init ();

  thread_current ()->dir = dir_open_root ();

  if (format)
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void)
{
  free_map_close ();
  cache_clear ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_dir)
{
  bool success = false;
  char *path = NULL;
  char *filename = NULL;
  struct dir *parent;
  struct inode *parent_inode;
  struct inode *inode;
  disk_sector_t sector;

  ASSERT (name != NULL);

  if (!dir_path_and_name (name, &path, &filename))
    return false;

  if (path == NULL)
    parent = dir_reopen (thread_current ()->dir);
  else
    parent = dir_parse (path);

  if (parent != NULL)
    {
      parent_inode = dir_get_inode (parent);

      if (!dir_lookup (parent, filename, &inode))
        {
          if (free_map_allocate (1, &sector))
            {
              if (is_dir)
                success = dir_create (sector, 0);
              else
                success = inode_create (sector, initial_size, false);
              success &= dir_add (parent, filename, sector);
            }
        }
      else
        inode_close (inode);

      dir_close (parent);
    }

  free (path);
  free (filename);
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  char *path = NULL;
  char *filename = NULL;
  struct dir *dir;
  struct inode *inode;

  ASSERT (name != NULL);

  if (!dir_path_and_name (name, &path, &filename))
    return NULL;

  if (path == NULL)
    dir = dir_reopen (thread_current ()->dir);
  else
    dir = dir_parse (path);

  if (dir != NULL)
    {
      if (filename[0] == 0)
        return file_open (dir_get_inode (dir));

      dir_lookup (dir, filename, &inode);
      dir_close (dir);
      return file_open (inode);
    }
  return NULL;
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name)
{
  char *path = NULL;
  char *filename = NULL;
  struct dir *dir;
  struct inode *inode;
  struct dir *target_dir;
  bool success = false;

  if (name == NULL)
    return false;

  if (!dir_path_and_name (name, &path, &filename))
    return false;

  if (path == NULL)
    dir = dir_reopen (thread_current ()->dir);
  else
    dir = dir_parse (path);

  if (dir != NULL)
    {
      /* Root directory. */
      if (filename[0] == 0)
        return false;

      dir_lookup (dir, filename, &inode);

      /* Current working directory. */
      if (inode == dir_get_inode (thread_current ()->dir))
        {
          dir_close (dir);
          inode_close (inode);
          return false;
        }

      if (inode_is_dir (inode))
        {
          target_dir = dir_open (inode);
          if (dir_empty (target_dir))
            {
              dir_close (target_dir);
              if (inode->open_cnt <= 1)
                success = dir_remove (dir, filename);
            }
          else
            dir_close (target_dir);
        }
      else
        success = dir_remove (dir, filename);

      dir_close (dir);
      inode_close (inode);
    }
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
