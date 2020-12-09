/*********** util.c file ****************/
#include "type.h"
#include <unistd.h>

extern off_t lseek(int fd, off_t offset, int whence);
extern  ssize_t read(int fd, void *buf, size_t count);
extern ssize_t write(int fd, const void *buf, size_t count);

extern MINODE minode[NMINODE];
extern MTABLE MountTable[NMOUNT];
extern char gpath[128]; // global for tokenized components
extern char *name[32];  // assume at most 32 components in pathname
extern int   nname;
extern MINODE *root;
extern int inode_start;
extern PROC *running;
extern int dev;

int get_block(int dev, int blk, char *buf)
{
   lseek(dev, (long)blk*BLKSIZE, 0);
   read(dev, buf, BLKSIZE);
}

int put_block(int dev, int blk, char *buf)
{
   lseek(dev, (long)blk*BLKSIZE, 0);
   write(dev, buf, BLKSIZE);
}

MINODE *mialloc() // allocate a FREE minode for use
{
  int i;

  for (i=0; i<NMINODE; i++){
    MINODE *mp = &minode[i];
    if (mp->refCount == 0){
      mp->refCount = 1;
      return mp;
    }
  }

  printf("FS panic: out of minodes\n");
  return 0;
}

int midalloc(MINODE *mip) // release a used minode
{
  mip->refCount = 0;
}

int tokenize(char *pathname)
{
  // copy pathname into gpath[]; tokenize it into name[0] to name[n-1]
  // Code in Chapter 11.7.2 
  char *s;
  strcpy(gpath, pathname);
  nname = 0;
  s = strtok(gpath, "/");
  while(s){
    name[nname++] = s;
    s = strtok(0, "/");
  }
}


MINODE *iget(int dev, int ino)
{
  // return minode pointer of loaded INODE=(dev, ino)
  // Code in Chapter 11.7.2
  MINODE *mip;
  MTABLE *mp;
  INODE *ip;
  int i, block, offset;
  char buf[BLKSIZE];

  // serach in-memory minodes first
  for (i=0; i<NMINODE; i++){
    MINODE *mip = &minode[i];
    if (mip->refCount && (mip->dev==dev) && (mip->ino==ino)){
      mip->refCount++;
      return mip;
    }
  }

  // needed INODE=(dev,ino) not in memory
  mip = mialloc(); // allocate a FREE minode
  mip->dev = dev; mip->ino = ino; // assign to (dev, ino)
  block = (ino-1)/8 + inode_start; // disk block containing this inode
  offset= (ino-1)%8; // which inode in this block
  get_block(dev, block, buf);
  ip = (INODE *)buf + offset;
  mip->inode = *ip; // copy inode to minode.INODE
  // initialize minode
  mip->refCount = 1;
  mip->mounted = 0;
  mip->dirty = 0;
  mip->mptr = 0;

  return mip;
}

void iput(MINODE *mip)
{
  // dispose of minode pointed by mip
  // Code in Chapter 11.7.2
  INODE *ip;
  int i, block, offset;
  char buf[BLKSIZE];

  if (mip==0) return;
  mip->refCount--; // dec refCount by 1
  if (mip->refCount > 0) return; // still has user
  if (mip->dirty == 0) return; // no need to write back

  // write INODE back to disk
  block = (mip->ino - 1) / 8 + inode_start;
  offset = (mip->ino - 1) % 8;

  // get block containing this inode
  get_block(mip->dev, block, buf);
  ip = (INODE *)buf + offset; // ip points at INODE
  *ip = mip->inode; // copy INODE to inode in block
  put_block(mip->dev, block, buf); // write back to disk
  midalloc(mip); // mip->refCount = 0;
} 

int search(MINODE *mip, char *name)
{
  // search for name in (DIRECT) data blocks of mip->INODE
  // return its ino
  // Code in Chapter 11.7.2
  int i;
  char *cp, temp[256], sbuf[BLKSIZE];
  DIR *dp;

  for (i=0; i<12; i++){ // search DIR direct blocks only
    if (mip->inode.i_block[i] == 0)
      return 0;
    get_block(mip->dev, mip->inode.i_block[i], sbuf);
    dp = (DIR *)sbuf;
    cp = sbuf;
    while (cp < sbuf + BLKSIZE){
      strncpy(temp, dp->name, dp->name_len);
      temp[dp->name_len] = 0;
      printf("%8d%8d%8u %s\n",
      dp->inode, dp->rec_len, dp->name_len, temp);
      if (strcmp(name, temp)==0){
        printf("found %s : inumber = %d\n", name, dp->inode);
        return dp->inode;
      }
      cp += dp->rec_len;
      dp = (DIR *)cp;
    }
  }
  return 0;
}

int getino(int *device,char *pathname)
{
  // return ino of pathname
  // Code in Chapter 11.7.2
  MINODE *mip;
  int ino;
  if (strcmp(pathname, "/")==0){
    return 2; // return root ino=2
  }
  if (pathname[0] == '/')
    mip = root; // if absolute pathname: start from root
  else
    mip = running->cwd; // if relative pathname: start from CWD
  mip->refCount++; // in order to iput(mip) later
  ino = mip->ino;
  *device = mip->dev;

  tokenize(pathname); // assume: name[ ], nname are globals

  /*for (i=0; i<nname; i++){ // search for each component string
    if (!S_ISDIR(mip->inode.i_mode)){ // check DIR type
      printf("%s is not a directory\n", name[i]);
      iput(mip);
      return 0;
    }
    ino = search(mip, name[i]);
    if (!ino){
      printf("no such component name %s\n", name[i]);
      iput(mip);
      return 0;
    }
    iput(mip); // release current minode
    mip = iget(device, ino); // switch to new minode
  }
  iput(mip);
  return ino;*/


  for(int i=0;i<nname;i++)
  {
      mip = iget(*device,ino);
      if(ino==2 && *device!=root->dev && (strcmp(name[i],"..")==0))
      {
          for(int j=0;j<NMOUNT;j++)
          {
              if(MountTable[j].dev==*device)
              {
                  *device=MountTable[j].mntDirPtr->dev;
                  ino=MountTable[j].mntDirPtr->ino;
                  mip=iget(*device,ino);
              }
          }
      }
      else if(mip->mounted == 1)
      {
          MTABLE * p = mip->mptr;
          *device = p->dev;
          iput(mip);
          mip = iget(*device,2);
      }
      ino = search(mip,name[i]);
      if(ino==0)
      {
          printf("%s does not exist.\n",name[i]);
          iput(mip);
          return 0;
      }

      if((mip->inode.i_mode & 0040000) != 0040000)
      {
          printf(" %s is not a directory!\n",name[i]);
          iput(mip);
          return 0;
      }
      iput(mip);
  }
  mip = iget(*device, ino);
  if(mip->mounted == 1)
  {
      MTABLE * p = mip->mptr;
      *device = p->dev;
      ino = 2;
  }
  iput(mip);
  return ino;
}

int findmyname(MINODE *parent, u32 myino, char *myname) 
{
  // WRITE YOUR code here:
  // search parent's data block for myino;
  // copy its name STRING to myname[ ];
  /*char buf[BLKSIZE];
  DIR *dp;
  char *cp;
  int parent_ino = 0;
  
  // Assume DIR has only one data block i_block[0]
  get_block(dev, parent->inode.i_block[0], buf);
  dp = (DIR *)buf;
  cp = buf;
  
  while (cp < buf + BLKSIZE){
    if (dp->inode == myino) {
      strncpy(myname, dp->name, dp->name_len);
      myname[dp->name_len] = 0;
    }

    cp += dp->rec_len;
    dp = (DIR *)cp;
  }*/
  int i;
  char buf[BLKSIZE], namebuf[256];
  char *cp;
  DIR *dp;
   
  for(i = 0; i <= 11 ; i ++)
    {
      if(parent->inode.i_block[i] != 0)
	{
	
      get_block(parent->dev, parent->inode.i_block[i], buf);
	  dp = (DIR *)buf;
	  cp = buf;
     
      while(cp < &buf[BLKSIZE])
	    {
	      strncpy(namebuf,dp->name,dp->name_len);
	      namebuf[dp->name_len] = 0;
	
	      if(dp->inode == myino)
		{
		  strcpy(myname, namebuf);
		  return 0;
		}
	      cp +=dp->rec_len;
	      dp = (DIR *)cp;
	    }
	}    
    }     
  return -1;


}

int findino(MINODE *mip, u32 *myino) // myino = ino of . return ino of ..
{
  // mip->a DIR minode. Write YOUR code to get mino=ino of .
  //                                         return ino of ..
  /*char buf[BLKSIZE], temp[256];
  DIR *dp;
  char *cp;
  int parent_ino = 0;
  
  // Assume DIR has only one data block i_block[0]
  get_block(dev, mip->inode.i_block[0], buf);
  dp = (DIR *)buf;
  cp = buf;
  
  while (cp < buf + BLKSIZE){
    strncpy(temp, dp->name, dp->name_len);
    temp[dp->name_len] = 0;
    
    if (temp[0] == '.' && temp[1] == 0) {
      // found ino of .
      if (myino) {
        *myino = dp->inode;
      }
    }
    else if (temp[0] == '.' && temp[1] == '.' && temp[2] == 0) {
      // found ino of ..
      parent_ino = dp->inode;
    }


    cp += dp->rec_len;
    dp = (DIR *)cp;
  }

  return parent_ino;*/
  int i;
  char buf[BLKSIZE], namebuf[256];
  char *cp;

  get_block(mip->dev, mip->inode.i_block[0], buf);
  dp = (DIR *)buf;
  cp = buf;
     
  *myino = dp->inode;
  cp +=dp->rec_len;
  dp = (DIR *)cp;
  return dp->inode;
}
