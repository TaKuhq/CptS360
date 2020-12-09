/****************************************************************************
*                   KCW testing ext2 file system                            *
*****************************************************************************/
#include "type.h"

// globals
MINODE minode[NMINODE];
MINODE *root;
MTABLE MountTable[NMOUNT];

PROC   proc[NPROC], *running;

char gpath[128]; // global for tokenized components
char *name[32];  // assume at most 32 components in pathname
int   nname;         // number of component strings

int fd, dev,old_dev;
int nblocks, ninodes, bmap, imap, inode_start;
int device;

int quit(void);
int init()
{
  int i, j;
  MINODE *mip;
  PROC   *p;

  printf("init()\n");

  for (i=0; i<NMINODE; i++){
    mip = &minode[i];
    mip->dev = mip->ino = 0;
    mip->refCount = 0;
    mip->mounted = 0;
    mip->mptr = 0;
  }
  for (i=0; i<NPROC; i++){
    p = &proc[i];
    p->pid = i;
    p->uid = p->gid = 0;
    p->cwd = 0;
    p->status = FREE;
    for (j=0; j<NFD; j++)
      p->fd[j] = 0;
  }

  for(i=0;i<NMOUNT;i++)
  {
      MountTable[i].dev = 0;
  }
}

// load root INODE and set root pointer to it
int mount_root()
{  
  printf("mount_root()\n");
  root = iget(dev, 2);
}

char *disk = "diskimage";
int main(int argc, char *argv[ ])
{
  int ino;
  char buf[BLKSIZE];
  char line[128], cmd[32], pathname[128], new_path[128];

  if (argc > 1)
    disk = argv[1];

  printf("checking EXT2 FS ....");
  if ((fd = open(disk, O_RDWR)) < 0){
    printf("open %s failed\n", disk);
    exit(1);
  }
  dev = fd;
  old_dev = fd;
  device = fd;

  /********** read super block  ****************/
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;

  /* verify it's an ext2 file system ***********/
  if (sp->s_magic != 0xEF53){
      printf("magic = %x is not an ext2 filesystem\n", sp->s_magic);
      exit(1);
  }     
  printf("EXT2 FS OK\n");
  ninodes = sp->s_inodes_count;
  nblocks = sp->s_blocks_count;

  get_block(dev, 2, buf); 
  gp = (GD *)buf;

  bmap = gp->bg_block_bitmap;
  imap = gp->bg_inode_bitmap;
  inode_start = gp->bg_inode_table;
  printf("bmp=%d imap=%d inode_start = %d\n", bmap, imap, inode_start);

  init();  
  mount_root();
  printf("root refCount = %d\n", root->refCount);

  printf("creating P0 as running process\n");
  running = &proc[0];
  running->status = READY;
  running->cwd = iget(dev, 2);
  printf("root refCount = %d\n", root->refCount);

  while(1){
    printf("input command : [ls|cd|pwd|mkdir|create|rmdir|link||unlink|symlink|readlink|open|close|read|write|cat|cp|mount|umount|quit] ");
    fgets(line, 128, stdin);
    line[strlen(line)-1] = 0;

    if (line[0]==0)
       continue;
    pathname[0] = 0;
    new_path[0] = 0;

    sscanf(line, "%s %s %s", cmd, pathname, new_path);
    printf("cmd=%s pathname=%s, new_path=%s\n", cmd, pathname, new_path);
  
    if (strcmp(cmd, "ls")==0)
       ls(pathname);
    if (strcmp(cmd, "cd")==0)
       chdir(pathname);
    if (strcmp(cmd, "pwd")==0)
       pwd(running->cwd);
    if (strcmp(cmd, "mkdir")==0)
       make_dir(pathname);
    if (strcmp(cmd, "create")==0)
      create(pathname);
    if (strcmp(cmd, "rmdir")==0)
      rmdir(pathname);
    if (strcmp(cmd, "link")==0)
      link(pathname, new_path);
    if(strcmp(cmd,"unlink")==0)
        unlink(pathname);
    if(strcmp(cmd,"symlink")==0)
        symlink(pathname,new_path);
    if(strcmp(cmd,"readlink")==0)
        readlink(pathname,new_path);
    if(strcmp(cmd,"open")==0)
        file_open(pathname,atoi(new_path));
    if(strcmp(cmd,"close")==0)
        file_close(atoi(pathname));
    if(strcmp(cmd,"read")==0)
    {
        char buf[1025] = {0};
        file_read(atoi(pathname),buf,1024);
        printf("%s\n",buf);
    }
    if(strcmp(cmd,"write")==0)
        file_write(atoi(pathname),"123456",6);
    if(strcmp(cmd,"cat")==0)
        file_cat(pathname);
    if(strcmp(cmd,"cp")==0)
        file_cp(pathname,new_path);
    if(strcmp(cmd,"mount")==0)
        my_mount(pathname,new_path);
    if(strcmp(cmd,"umount")==0)
        my_umount(pathname);

    if (strcmp(cmd, "quit")==0)
       quit();
  }
}

int quit(void)
{
  int i;
  MINODE *mip;
  for (i=0; i<NMINODE; i++){
    mip = &minode[i];
    if (mip->refCount > 0)
      iput(mip);
  }
  exit(0);
}
