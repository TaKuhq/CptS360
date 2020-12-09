#include "type.h"

extern int dev;
extern int imap,bmap;

int clr_bit(char *buf, int bit) // clear bit in char buf[BLKSIZE]
{
  buf[bit/8] &= ~(1 << (bit%8));
}

int incFreeInodes(int dev)
{
    char buf[BLKSIZE];
    // inc free inodes count in SUPER and GD
    get_block(dev, 1, buf);
    sp = (SUPER *)buf;
    sp->s_free_inodes_count++;
    put_block(dev, 1, buf);
    get_block(dev, 2, buf);
    gp = (GD *)buf;
    gp->bg_free_inodes_count++;
    put_block(dev, 2, buf);
}

int incFreeBnodes(int dev)
{
    char buf[BLKSIZE];
    // inc free inodes count in SUPER and GD
    get_block(dev, 1, buf);
    sp = (SUPER *)buf;
    sp->s_free_blocks_count++;
    put_block(dev, 1, buf);
    get_block(dev, 2, buf);
    gp = (GD *)buf;
    gp->bg_free_blocks_count++;
    put_block(dev, 2, buf);
}

int idalloc(int dev, int ino)
{
  int i;
  char buf[BLKSIZE];

  // get inode bitmap block
  get_block(dev, imap, buf);
  clr_bit(buf, ino-1);
  // write buf back
  put_block(dev, imap, buf);
  incFreeInodes(dev);
}

int bdalloc(int dev, int bno)
{
  int i;
  char buf[BLKSIZE];

  // get block bitmap block
  get_block(dev, bmap, buf);
  clr_bit(buf, bno-1);
  // write buf back
  put_block(dev, bmap, buf);
    incFreeBnodes(dev);
}


int rm_child(MINODE *pip, char *child)
{
    int i, size, found = 0;
        char *cp, *cp2;
        DIR *dp, *dp2, *dpPrev;
        char buf[BLKSIZE], buf2[BLKSIZE], temp[256];
        memset(buf2, 0, BLKSIZE);
        for(i = 0; i < 12; i++)
        {
            if(pip->inode.i_block[i] == 0) { return 0; }
            get_block(pip->dev, pip->inode.i_block[i], buf);
            dp = (DIR *)buf;
            dp2 = (DIR *)buf;
            dpPrev = (DIR *)buf;
            cp = buf;
            cp2 = buf;
            while(cp < buf+BLKSIZE && !found)
            {
                memset(temp, 0, 256);
                strncpy(temp, dp->name, dp->name_len);
                if(strcmp(child, temp) == 0)
                {
                    //if child is only entry in block
                    if(cp == buf && dp->rec_len == BLKSIZE)
                    {
                    bdalloc(pip->dev, pip->inode.i_block[i]);
                    pip->inode.i_block[i] = 0;
                    pip->inode.i_blocks--;
                    found = 1;
                    }
                    //else delete child and move entries over left
                    else
                    {
                        while((dp2->rec_len + cp2) < buf+BLKSIZE)
                        {
                            dpPrev = dp2;
                            cp2 += dp2->rec_len;
                            dp2 = (DIR*)cp2;
                        }
                        if(dp2 == dp) //Child is last entry
                        {
                            //printf("Child is last entry\n"); //FOR TESTING
                            dpPrev->rec_len += dp->rec_len;
                            found = 1;
                        }
                        else
                        {
                            //printf("Child is not the last entry\n"); //FOR TESTING
                            size = ((buf + BLKSIZE) - (cp + dp->rec_len));
                            printf("Size to end = %d\n", size);
                            dp2->rec_len += dp->rec_len;
                            printf("dp2 len = %d\n", dp2->rec_len);
                            memmove(cp, (cp + dp->rec_len), size);
                            dpPrev = (DIR*)cp;
                            memset(temp, 0, 256);
                            strncpy(temp, dpPrev->name, dpPrev->name_len);
                            printf("new dp name = %s\n", temp);
                            found = 1;
                        }
                    }
                }
                cp += dp->rec_len;
                dp = (DIR*)cp;
            }
            if(found)
            {
            put_block(pip->dev, pip->inode.i_block[i], buf);
            return 1;
            }
        }
        printf("ERROR: CHILD NOT FOUND\n");
        return -1;
}

int rmdir(char *pathname)
{
  int ino, pino;
  MINODE *mip, *pip;
  char myname[256];

  // 1.  get in-memory INODE of pathname:
  ino = getino(&dev, pathname);
  mip = iget(dev, ino);

  if (!S_ISDIR(mip->inode.i_mode)) {
    printf("Not a Dir!!\n");
    iput(mip);
    return -1;
  }

  if(mip->refCount>1)
  {
      printf("minode is BUSY\n");
      iput(mip);
      return -1;
  }

  if (mip->inode.i_links_count>2) {
    printf("is not empty!\n");
    iput(mip);
    return -1;
  }

//  for (int i=0; i<12; i++){
//    if (mip->inode.i_block[i]==0)
//        continue;
//    bdalloc(mip->dev, mip->inode.i_block[i]);
//   }
//   idalloc(mip->dev, mip->ino);
//   mip->refCount = 0;
//   iput(mip);

  // get parent's ino and inode
  pino = findino(mip, NULL);
  pip = iget(mip->dev, pino);

  // get name from parent DIR's data block
  findmyname(pip, ino, myname);

  // remove name from parent directory
  rm_child(pip, myname);

  // dec parent links_count by 1; mark paren pimp dirty
  iput(pip);

  // deallocate its data blocks and inode
  bdalloc(dev, mip->inode.i_block[0]);
  idalloc(dev, ino);
  iput(mip);

}
