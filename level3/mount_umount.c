#include "type.h"
extern MTABLE MountTable[NMOUNT];
extern MINODE minode[NMINODE];
extern int dev;
PROC *running;
MINODE *root;

int pmt()
{
    int i;
    printf("********** Mount Table **********\n");
    printf("dev ino name\n");
    printf("--- --- ----------\n");
    for(i = 0; i < NMOUNT; i++)
    {
        if(MountTable[i].dev != 0)
        {
            printf("%3d %3d %s\n", MountTable[i].dev, MountTable[i].mntDirPtr->ino, MountTable[i].mntName);
        }
    }
    return 1;
}

int my_mount(char *filesys,char *mount_point)    /*  Usage: mount filesys mount_point OR mount */
{
  int i,fd;
  int dev, ino;
  MINODE *mip;
  char buf[BLKSIZE];
  int magic,nblocks, bfree, ninodes, ifree;

  if(strlen(filesys)== 0 && strlen(mount_point) == 0)
    {
      pmt();
      return 0;

    }


  //2. Check whether filesys is already mounted:
  for(i=0;i < NMOUNT;i++)
    {
      if((strcmp(MountTable[i].mntName,filesys)==0))
    {
      printf("%s is already mounted\n",filesys);
      return -1;
    }
    }

  i = 0;
  while(MountTable[i].dev)
    i++;

  fd = open(filesys,O_RDWR);

  if(fd < 0)
    {
      printf("open %s failed.\n",filesys);
      return -1;
    }

  // verify it is an EXT2 FS
  get_block(fd,1,buf);

  sp = (SUPER *)buf;
  magic = sp->s_magic;
  nblocks = sp->s_blocks_count;
  bfree = sp->s_free_blocks_count;
  ninodes = sp->s_inodes_count;
  ifree = sp->s_free_inodes_count;


 if(magic != 0xEF53)
   {
     printf("SUPER_MAGIC = %d it is not an EX2 Filesystem.\n",magic);
     return -1;
   }

  get_block(fd,2,buf);
  gp = (GD *)buf;
  printf("iblock = %d\n",gp->bg_inode_table);
  printf("bmap=%d\n",gp->bg_block_bitmap);
  printf("imap=%d\n",gp->bg_inode_bitmap);

  if(*mount_point == '/')
    dev = root->dev;
  else
    dev = running->cwd->dev;

  ino = getino(&dev,mount_point);

  if(!ino)
    return -1;

  mip = iget(dev, ino);

  if((mip->inode.i_mode & 0040000) != 0040000 || mip->refCount > 1)
    return -1;

  MountTable[i].ninodes = ninodes;
  MountTable[i].nblocks = nblocks;
  MountTable[i].dev = fd;
  MountTable[i].mntDirPtr = mip;
  mip->mounted = 1;
  mip->mptr = &(MountTable[i]);
  strcpy(MountTable[i].mntName,filesys);
  strcpy(MountTable[i].devName, mount_point);

  printf("mount : %s mounted on %s \n",filesys,mount_point);
  printf("nblocks=%d bfree=%d  ninodes=%d ifree=%d\n",nblocks, bfree,ninodes,ifree);

   return 0;

}

int my_umount(char *pathname)
{
  int i;
  int dev;
  int mounted_checker=0;
  MTABLE *temp;
  int freeBlocks, freeInodes;
  char buf[BLKSIZE];

   //1. Search the MOUNT table to check filesys is indeed mounted.
   for(i=0;i<NMOUNT;i++)
     {
       if(strcmp(MountTable[i].mntName,pathname)==0)
     {
       printf("umount : %s\n",MountTable[i].mntName);
       dev=MountTable[i].dev;
       mounted_checker=1;
       temp=&(MountTable[i]);
       break;
     }
     }

   if(mounted_checker==0)
     {
       return -1;
     }

   //2. Check whether any file is still active in the mounted filesys;
   //      e.g. someone's CWD or opened files are still there,
   //   if so, the mounted filesys is BUSY ==> cannot be umounted yet.
   //   HOW to check?      ANS: by checking all minode[].dev

   for(i=0;i<NMINODE;i++)
     {
       if(minode[i].refCount!=0 && minode[i].dev==dev)
     {
       printf("the mounted filesysis BUSY, canoot be unmounted yet!!\n");
       return -1;
     }
     }

   //3. Find the mount_point's inode (which should be in memory while it's mounted
   //   on).  Reset it to "not mounted"; then
   //         iput()   the minode.  (because it was iget()ed during mounting)
   get_block(temp->dev,1,buf);
   sp = (SUPER *)buf;
   freeBlocks = sp->s_free_blocks_count;
   freeInodes = sp->s_free_inodes_count;
   printf("nblocks=%4d freeBlocks=%4d usedBlocks=%4d\n",temp->nblocks, freeBlocks, temp->nblocks-freeBlocks);
   printf("ninodes=%4d freeInodes=%4d usedInodes=%4d\n",temp->ninodes, freeInodes, temp->ninodes-freeInodes);
   temp->mntDirPtr->mounted=0;
   iput(temp->mntDirPtr);
   temp->dev = 0;
   printf("%s unmounted\n",pathname);

   return 0;

}
