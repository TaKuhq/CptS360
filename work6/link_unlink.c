#include "type.h"

extern int enter_name(MINODE *pip, int myino, char *myname);
extern int dev;
extern int imap,bmap;
PROC *running;

int link(char *oldpath, char *new_path)
{

  int oino, nino, pino;
  MINODE *omip, *nmip, *pmip;
  char *parent, *child;

  if (oldpath == NULL ||
    new_path == NULL) {
    return -1;
  }

  if (strlen(oldpath) == 0
    || strlen(new_path) == 0) {
    return -1;
  }

  // verify old_file exists and is not a DIR;
  oino = getino(oldpath);
  omip = iget(dev, nino);

  if (S_ISDIR(omip->inode.i_mode)) {
    // must not be a DIR
    return -1;
  }

  // new_file must not exist yet
  if (getino(new_path) != 0) {
    return -1;
  }

  // creat new_file with the same inode number of old_file
  parent = dirname(new_path);
  child = basename(new_path);
  pino = getino(parent);
  pmip = iget(dev, pino);

  // creat entry in new parent DIR with same inode number of old_file
  omip->inode.i_mode &= (~0xF000);
  omip->inode.i_mode |= 0xA000;
  enter_name(pmip, oino, child);

  omip->inode.i_links_count++; // inc INODE’s links_count by 1
  omip->dirty = 1; // for write back by iput(omip)
  iput(omip);
  iput(pmip);

}


int unlink(char *filename)
{
    int ino, pino;
    MINODE *mip, *pmip;
    char *parent, *child;
    ino = getino(filename);
    mip = iget(dev, ino);

    parent = dirname(filename);
    child = basename(filename);
    pino = getino(parent);
    pmip = iget(dev, pino);
    rm_child(pmip, child);
    pmip->dirty = 1;
    iput(pmip);
    mip->inode.i_links_count--;
    if (mip->inode.i_links_count > 0)
        mip->dirty = 1;
    else{
        bdalloc(dev, mip->inode.i_block[0]);
        idalloc(dev, ino);
    }
    iput(mip);
}

int symlink(char *oldpath, char *new_path)
{
    int ino;
    MINODE *mip;
    if(0 >= (ino = getino(oldpath)))
    {
        printf("ERROR: FILE DOES NOT EXIST\n");
        return -1;
    }
    create(new_path);
    if(0 >= (ino = getino(new_path)))
    {
        printf("ERROR: COULD NOT CREATE FILE\n");
        return -1;
    }
    mip = iget(dev, ino);
    mip->inode.i_mode &= ~0770000;
    mip->inode.i_mode |= 0120000;
    mip->dirty = 1;
    strcpy((char*)mip->inode.i_block, oldpath);
    iput(mip);
}

int readlink(file, buffer)
{
    int ino;
    MINODE *mip;
    ino = getino(file);
    mip = iget(dev, ino);
    findmyname(mip, ino, buffer);
    return mip->inode.i_size;
}
