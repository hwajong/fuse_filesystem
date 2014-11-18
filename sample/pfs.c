/*
 * OS Assignment #4
 *
 * @file pfs.c
 */

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

static int
pfs_getattr (const char  *path,
             struct stat *stbuf)
{
  /* not yet implemented */
  return -ENOENT;
}

static int
pfs_readdir (const char            *path,
             void                  *buf,
             fuse_fill_dir_t        filler,
             off_t                  offset,
             struct fuse_file_info *fi)
{
  /* not yet implemented */
  return -ENOENT;
}

static int pfs_unlink (const char *path)
{
  /* not yet implemented */
  return -ENOENT;
}

static struct fuse_operations pfs_oper =
{
  .getattr  = pfs_getattr,
  .readdir  = pfs_readdir,
  .unlink   = pfs_unlink
};

int
main (int    argc,
      char **argv)
{
  return fuse_main (argc, argv, &pfs_oper);
}
