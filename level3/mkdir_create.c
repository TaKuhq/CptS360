/************* mkdir_create.c file **************/
#include "type.h"

extern int nblocks, ninodes, bmap, imap, inode_start;
extern MINODE *root;
extern PROC *running;
extern int dev;

int tst_bit(char *buf, int bit)
{
  // in Chapter 11.3.1
  return buf[bit/8] & (1 << (bit % 8));
}

int set_bit(char *buf, int bit)
{
  // in Chapter 11.3.1
  buf[bit/8] |= (1 << (bit % 8));
}

int decFreeInodes(char *buf, int dev)
{
    // dec free inodes count in SUPER and GD
    get_block(dev, 1, buf);
    sp = (SUPER *)buf;
    sp->s_free_inodes_count--;
    put_block(dev, 1, buf);
    get_block(dev, 2, buf);
    gp = (GD *)buf;
    gp->bg_free_inodes_count--;
    put_block(dev, 2, buf);
}

int decFreeBnodes(char *buf, int dev)
{
    // dec free inodes count in SUPER and GD
    get_block(dev, 1, buf);
    sp = (SUPER *)buf;
    sp->s_free_blocks_count--;
    put_block(dev, 1, buf);
    get_block(dev, 2, buf);
    gp = (GD *)buf;
    gp->bg_free_blocks_count--;
    put_block(dev, 2, buf);
}

int ialloc(int dev)  // allocate an inode number from inode_bitmap
{
  int  i;
  char buf[BLKSIZE];

// read inode_bitmap block
  get_block(dev, imap, buf);

  for (i=0; i < ninodes; i++){
    if (tst_bit(buf, i)==0){
        set_bit(buf, i);
        put_block(dev, imap, buf);
        decFreeInodes(buf,dev);
        printf("allocated ino = %d\n", i+1); // bits count from 0; ino from 1
        return i+1;
    }
  }

  // out of FREE inodes
  return 0;
}

int balloc(int dev)
{
  int  i;
  char buf[BLKSIZE];

// read block_bitmap block
  get_block(dev, bmap, buf);

  for (i=0; i < nblocks; i++){
    if (tst_bit(buf, i)==0){
        set_bit(buf, i);
        put_block(dev, bmap, buf);
        decFreeBnodes(buf,dev);
        printf("allocated block = %d\n", i+1); // bits count from 0; block from 1
        return i+1;
    }
  }

  // out of FREE blocks
  return 0;

}

 int enter_name(MINODE *pip, int myino, char *myname)
{
  char buf[BLKSIZE], temp[256];
  DIR *dp;
  char *cp;
  int i = 0;

  //  For each data block of parent DIR do { // assume: only 12 direct blocks
  for (i = 0; i < 12; i++) {
    int ideal_len, need_len, remain;
    if (pip->inode.i_block[i] == 0) {
      break;
    }

    //  get parent's data block into a buf[ ]
    get_block(pip->dev, pip->inode.i_block[i], buf);

    dp = (DIR *)buf;
    cp = buf;

    // step to LAST entry in block: 
    int blk = pip->inode.i_block[i];
    printf("step to LAST entry in data block %d\n", blk);
    while (cp + dp->rec_len < buf + BLKSIZE){
    
      /*************************************************
        print DIR record names while stepping through
      **************************************************/

      cp += dp->rec_len;
      dp = (DIR *)cp;
    } 
    // dp NOW points at last entry in block

    ideal_len = 4 * ((8 * dp->name_len + 3) / 4);
    need_len = 4 * ((8 * strlen(myname) + 3) / 4);
    remain = dp->rec_len - ideal_len;
    if (remain >= need_len) {
      dp->rec_len = ideal_len;
      cp += dp->rec_len;
      dp = (DIR *)cp;
      dp->inode = myino;
      dp->rec_len = remain;
      dp->name_len = strlen(myname);
      dp->file_type = 0;
      strcpy(dp->name, myname);
      goto enter_name_done;
    }

  }

  printf(" NO space in existing data block, allocate a new data block!\n");
  int bno = balloc(dev);
  bzero(buf, BLKSIZE); // optional: clear buf[ ] to 0
  pip->inode.i_block[i] = bno;
  pip->inode.i_size += BLKSIZE;
  dp = (DIR *)buf;
  dp->inode = myino;
  dp->rec_len = BLKSIZE;
  dp->name_len = strlen(myname);
  dp->file_type = 0;
  strcpy(dp->name, myname);

enter_name_done:
  put_block(dev, pip->inode.i_block[i], buf);

  return 0;
}




int mymkdir(MINODE *pip, char *name)
{
  MINODE *mip;
  int ino, bno;

  /**
   * 1. pip points at the parent minode[] of "/a/b", name is a string "c" 
   *
   * 2. allocate an inode and a disk block for the new directory
   */
  ino = ialloc(dev);
  bno = balloc(dev);
  printf("ino %d and bno %d of the new dir\n", ino, bno);

  /**
   * 3. load the inode into a minode[] (in order to
   *    wirte contents to the INODE in memory.
   */
  mip = iget(dev,ino);

  /**
   * 4. Create inode
   */
  INODE *ip = &mip->inode;

  // Use ip-> to acess the INODE fields:
  ip->i_mode = 0x41ED;    // OR 040755: DIR type and permissions
  ip->i_uid  = running->uid;  // Owner uid 
  ip->i_gid  = running->gid;  // Group Id
  ip->i_size = BLKSIZE;   // Size in bytes 
  ip->i_links_count = 2;          // Links count=2 because of . and ..
  ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);  // set to current time
  ip->i_blocks = 2;                 // LINUX: Blocks count in 512-byte chunks 
  ip->i_block[0] = bno;             // new DIR has one data block
  // i_block[1] to i_block[14] = 0;
  for (int i = 1; i < 15; i++) {
    ip->i_block[i] = 0;
  }

  mip->dirty = 1;               // mark minode dirty
  iput(mip);                    // write INODE to


  /**
   * 5. Create data block for new DIR containing . and .. entries
   */
  char buf[BLKSIZE];
  bzero(buf, BLKSIZE); // optional: clear buf[ ] to 0
  DIR *dp = (DIR *)buf;
  // make . entry
  dp->inode = ino;
  dp->rec_len = 12;
  dp->name_len = 1;
  dp->name[0] = '.';
  // make .. entry: pino=parent DIR ino, blk=allocated block
  dp = (DIR *)((char *)dp + 12);
  dp->inode = pip->ino;
  dp->rec_len = BLKSIZE-12; // rec_len spans block
  dp->name_len = 2;
  dp->name[0] = dp->name[1] = '.';
  put_block(dev, bno, buf); // write to blk on diks

  /**
   * 6. Enter new dir_entry into parent directory
   */
  enter_name(pip, ino, name);

  return 0;
}


int make_dir(char *pathname)
{
  MINODE *start;
  char *parent, *child;
  int pino;
  MINODE *pip;

  /**
   * 1. pahtname = absolute: start = root;         dev = root->dev;
   *        = relative: start = running->cwd; dev = running->cwd->dev;
   */
  if (pathname[0] == '/') {
    // absolute
    start = root;
  }
  else {
    start = running->cwd;
  }

  /**
   * 2. Let  
   * parent = dirname(pathname);   parent= "/a/b" OR "a/b"
   * child  = basename(pathname);  child = "c"
   */
   parent = dirname(pathname);
   child = basename(pathname);
   printf("parent:%s, child:%s\n", parent, child);


  /**
   * 3. Get minode of parent:
   *
   *     pino  = getino(parent);
   *     pip   = iget(dev, pino); 
   *
   * Verify : (1). parent INODE is a DIR (HOW?)   AND
   *          (2). child does NOT exists in the parent directory (HOW?);
   */
  pino = getino(&dev,parent);
  pip = iget(dev, pino);

  if (!S_ISDIR(pip->inode.i_mode)){
    printf("Not a Dir!!\n");
    return -1;
  }

  if (search(pip,  child) != 0) {
    printf("child exists!\n");
    return -1;
  }

  mymkdir(pip, child);

  /**
   * 5. inc parent inodes's links count by 1; 
   *    touch its atime, i.e. atime = time(0L), mark it DIRTY
   */
   pip->inode.i_links_count++;    // increment the count by 1
   pip->dirty = 1;                // mark it DIRTY
   pip->inode.i_atime = time(0L); // touch its atime

  iput(pip);
}



int my_create(MINODE *pip, char *name)
{
  MINODE *mip;
  int ino, bno;

  /**
   * 1. pip points at the parent minode[] of "/a/b", name is a string "c" 
   *
   * 2. allocate an inode and a disk block for the new directory
   */
  ino = ialloc(dev);
  bno = balloc(dev);
  printf("ino %d and bno %d of the new dir\n", ino, bno);

  /**
   * 3. load the inode into a minode[] (in order to
   *    wirte contents to the INODE in memory.
   */
  mip = iget(dev,ino);

  /**
   * 4. Create inode
   */
  INODE *ip = &mip->inode;

  // Use ip-> to acess the INODE fields:
  ip->i_mode = 0x81A4;    // OR 080644: REG type and permissions
  ip->i_uid  = running->uid;  // Owner uid 
  ip->i_gid  = running->gid;  // Group Id
  ip->i_size = 0;   // Size in bytes 
  ip->i_links_count = 1;          // Links count=2 because of . and ..
  ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);  // set to current time
  ip->i_blocks = 2;                 // LINUX: Blocks count in 512-byte chunks 
  ip->i_block[0] = 0;             // new DIR has one data block
  // i_block[1] to i_block[14] = 0;
  for (int i = 1; i < 15; i++) {
    ip->i_block[i] = 0;
  }

  mip->dirty = 1;               // mark minode dirty
  iput(mip);                    // write INODE to


#if 0
  /**
   * 5. Create data block for new DIR containing . and .. entries
   */
  char buf[BLKSIZE];
  bzero(buf, BLKSIZE); // optional: clear buf[ ] to 0
  DIR *dp = (DIR *)buf;
  // make . entry
  dp->inode = ino;
  dp->rec_len = 12;
  dp->name_len = 1;
  dp->name[0] = '.';
  // make .. entry: pino=parent DIR ino, blk=allocated block
  dp = (DIR *)((char *)dp + 12);
  dp->inode = pip->ino;
  dp->rec_len = BLKSIZE-12; // rec_len spans block
  dp->name_len = 2;
  dp->name[0] = dp->name[1] = '.';
  put_block(dev, bno, buf); // write to blk on diks

#endif

  /**
   * 6. Enter new dir_entry into parent directory
   */
  enter_name(pip, ino, name);

  return 0;
}


int create(char *pathname)
{
  MINODE *start;
  char *parent, *child;
  int pino;
  MINODE *pip;

  /**
   * 1. pahtname = absolute: start = root;         dev = root->dev;
   *        = relative: start = running->cwd; dev = running->cwd->dev;
   */
  if (pathname[0] == '/') {
    // absolute
    start = root;
  }
  else {
    start = running->cwd;
  }

  /**
   * 2. Let  
   * parent = dirname(pathname);   parent= "/a/b" OR "a/b"
   * child  = basename(pathname);  child = "c"
   */
   parent = dirname(pathname);
   child = basename(pathname);
   printf("parent:%s, child:%s\n", parent, child);


  /**
   * 3. Get minode of parent:
   *
   *     pino  = getino(parent);
   *     pip   = iget(dev, pino); 
   *
   * Verify : (1). parent INODE is a DIR (HOW?)   AND
   *          (2). child does NOT exists in the parent directory (HOW?);
   */
  pino = getino(&dev,parent);
  pip = iget(dev, pino);

  if (search(pip,  child) != 0) {
    printf("child exists!\n");
    return -1;
  }

  my_create(pip, child);

  /**
   * 5.touch its atime, i.e. atime = time(0L), mark it DIRTY
   */
   pip->dirty = 1;                // mark it DIRTY
   pip->inode.i_atime = time(0L); // touch its atime

  iput(pip);
}

