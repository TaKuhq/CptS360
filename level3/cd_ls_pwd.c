/************* cd_ls_pwd.c file **************/

#include"type.h"
extern int dev;
extern int old_dev;
extern PROC *running;
extern MINODE *root;
MTABLE MountTable[NMOUNT];

int chdir(char *pathname)   
{
  printf("chdir %s\n", pathname);
  // READ Chapter 11.7.3 HOW TO chdir
  /*int ino;
  MINODE *mip;
  
  ino = getino(dev,pathname);
  if (ino == 0)
    return -1;

  mip = iget(dev, ino);

  if (!S_ISDIR(mip->inode.i_mode)){
    printf("Not a Dir!!\n");
    return -1;
  }

  iput(running->cwd);
  running->cwd = mip;*/

  int  ino;
  MINODE *mp;
 
  // cd to the root
  if(pathname[0] == 0)
    {
      iput(running->cwd);
      running->cwd = root;
      root->refCount++;
      return 0;
    }

  dev = running->cwd->dev;
  ino = getino(&dev, pathname);

  // no such directory
  if(ino == 0)
    return -1;
    
  mp = iget(dev, ino);

  // check DIR type
  if(((mp->inode.i_mode) & 0040000)!= 0040000)
    {
      printf("%s is not a directory\n",pathname);
      iput(mp);
      return -1;
    }

  // dispose of original CWD
  iput(running->cwd);

  running->cwd = mp;
  return 0;
}


int ls_file(MINODE *mip, char *name)
{
  // READ Chapter 11.7.3 HOW TO ls
  char buf[BLKSIZE], temp[256];
  MINODE *ip;
  char *cp;
  char *t1 = "xwrxwrxwr-------";
  char *t2 = "----------------";
  char ftime[64];
  time_t tm  = (time_t)mip->inode.i_atime;

  if (S_ISREG(mip->inode.i_mode)) {
    printf("%c", '-');
  }
  else if (S_ISDIR(mip->inode.i_mode)) {
    printf("%c", 'd');
  }
  else if (S_ISLNK(mip->inode.i_mode)) {
    printf("%c", 'l');
  }

  for (int i = 8; i >= 0; i--) {
    if (mip->inode.i_mode & (1 << i)) {
      printf("%c", t1[i]);  // print r|w|x
    }
    else {
      printf("%c", t2[i]);
    }
  }

  printf("%4d ", mip->inode.i_links_count); // link count
  printf("%4d ", mip->inode.i_gid); // gid
  printf("%4d ", mip->inode.i_uid); // uid
  printf("%8d ", mip->inode.i_size); // file size

  // print time
  strcpy(ftime, ctime(&tm));
  ftime[strlen(ftime) - 1] = '\0';    // kill \n at end
  printf("%s ",  ftime);

  // print name
  printf("%s ", name);
  if(S_ISLNK(mip->inode.i_mode))
  {
      printf("-> %s", (char*)mip->inode.i_block);
  }

  printf("\n");
  iput(mip);
}

int ls_dir(int device,MINODE *mip)
{
  char buf[BLKSIZE], temp[256];
  DIR *dp;
  char *cp;
  MINODE *submip;

  // Assume DIR has only one data block i_block[0]
  get_block(device, mip->inode.i_block[0], buf);
  dp = (DIR *)buf;
  cp = buf;
  
  while (cp < buf + BLKSIZE){
    strncpy(temp, dp->name, dp->name_len);
    temp[dp->name_len] = 0;

    // printf("[%d %s]  ", dp->inode, temp); // print [inode# name]
    submip = iget(device, dp->inode);
    ls_file(submip, temp);

    cp += dp->rec_len;
    dp = (DIR *)cp;
  }
  printf("\n");
}

int ls(char *pathname)  
{
  int ino;
  MINODE *mip;
  
  printf("ls %s\n", pathname);

  if (pathname[0] == 0) {
      mip = iget(dev,running->cwd->ino);
      ls_dir(dev,running->cwd);
      iput(mip);
      return 0;
  }

  int device = dev;
  ino = getino(&device,pathname);
  printf("ino :%d\n", ino);
  mip = iget(dev, ino);

  if (S_ISDIR(mip->inode.i_mode))
    ls_dir(device,mip);
  else
  {
    ls_file(mip, pathname);
  }

  return 0;
}

int count = 0;
void rpwd(MINODE *wd)
{
  /*int my_ino, parent_ino;
  MINODE *pip;
  char myname[256];

  if (wd == root) {
    return;
  }

  parent_ino = findino(wd, &my_ino);
  printf("my_ino:%d, parent_ino:%d\n", my_ino, parent_ino);
  pip = iget(dev, parent_ino);

  findmyname(pip, my_ino, myname);
  rpwd(pip);
  printf("/%s", myname);*/

  struct DIR *dirp;
  char myname[256];
  unsigned long myino,parentino;
  int f;
  
  
  if(wd == root)
    {
      return;
    }
  
  parentino = findino(wd,&myino);
  if(myino==2 && wd->dev!=root->dev)
  {
      for(f=0;f<NMOUNT;f++)
      {
          if(MountTable[f].dev==wd->dev)
          {
            dev=MountTable[f].mntDirPtr->dev;
            parentino=MountTable[f].mntDirPtr->ino;
            wd = iget(dev, parentino);
            rpwd(wd);
            iput(wd);
            return;
          }
       }
  }
  //printf("%d-%d--%d\n",myino,parentino,count++);
  wd = iget(wd->dev, parentino);
  rpwd(wd);
  findmyname(wd,myino,myname);
  if(strcmp(myname,"..")!=0 && strcmp(myname,".")!=0)
    printf("/%s",myname);
  iput(wd);
}

char *pwd(MINODE *wd)
{
  if (wd == root){
    printf("/\n");
  }
  else {
    rpwd(wd);
    printf("\n");
  }

  return NULL;
}



