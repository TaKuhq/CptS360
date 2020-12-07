#include "type.h"
#include <malloc.h>

extern int dev;
extern PROC *running;

int truncate(MINODE *mip)
{
    int buf[256];
    int buf2[256];
    int bnumber, i, j;
    if(mip == NULL)
    {
        printf("ERROR: NO FILE\n");
        return -1;
    }
    // Deallocate all used blocks
    for(i = 0; i < 12; i++) //Check direct blocks
    {
        if(mip->inode.i_block[i] != 0)
        {
            bdalloc(mip->dev, mip->inode.i_block[i]);
        }
    }
    //Indirect blocks
    if(mip->inode.i_block[12] == 0) {return 1;}
    get_block(dev, mip->inode.i_block[12], (char*)buf);
    for(i = 0; i < 256; i++)
    {
        if(buf[i] != 0) {bdalloc(mip->dev, buf[i]);}
    }
    bdalloc(mip->dev, mip->inode.i_block[12]);
    if(mip->inode.i_block[13] == 0) {return 1;}
    //deallocate all double indirect blocks
    memset(buf, 0, 256);
    get_block(mip->dev, mip->inode.i_block[13], (char*)buf);
    for(i = 0; i < 256; i++)
    {
        if(buf[i])
        {
            get_block(mip->dev, buf[i], (char*)buf2);
            for(j = 0; j < 256; j++)
            {
                if(buf2[j] != 0) {bdalloc(mip->dev, buf2[j]);}
            }
            bdalloc(mip->dev, buf[i]);
        }
    }
    bdalloc(mip->dev, mip->inode.i_block[13]);
    mip->inode.i_atime = mip->inode.i_mtime = time(0L);
    mip->inode.i_size = 0;
    mip->dirty = 1;
    return 0;
}

int file_open(char *filename, int flags)
{
    int ino;
    MINODE *mip;
    OFT *oftp;

    ino = getino(filename);
    if (ino==0){ // if file does not exist
        create(filename); // creat it first, then
        ino = getino(filename); // get its ino
    }
    mip = iget(dev, ino);

    oftp = (OFT*)malloc(sizeof(OFT));
    oftp->mode = flags;
    oftp->refCount = 1;
    oftp->mptr = mip;
    switch(flags)
    {
        case 0: oftp->offset = 0;
            break;
        case 1: truncate(mip);
            oftp->offset = 0;
            break;
        case 2: oftp->offset = 0;
            break;
        case 3: oftp->offset = mip->inode.i_size;
            break;
        default: printf("ERROR: INVALID MODE\n");
            iput(mip);
            free(oftp);
            return -1;
            break;
    }

    int i = 0;
    while(running->fd[i] != NULL && i < 10) { i++; }
    if(i == 10)
    {
        printf("ERROR: NO ROOM TO OPEN FILE\n");
        iput(mip);
        free(oftp);
        return -1;
    }
    running->fd[i] = oftp;
    if(flags != 0) { mip->dirty = 1; }
    printf("file opened,fd=%d\n",i);
    return i;
}

int file_close(int fd)
{
    OFT *oftp;
    MINODE *mip;
    if (fd > 9 || fd < 0)
    {
        printf("ERROR: FILE DESCRIPTOR OUT OF RANGE\n");
        return -1;
    }
    if(running->fd[fd] == NULL)
    {
        printf("ERROR: FILE DESCRIPTOR NOT FOUND\n");
        return -1;
    }
    oftp = running->fd[fd];
    running->fd[fd] = 0;
    oftp->refCount--;
    if(oftp->refCount > 0) {return 1;}
    mip = oftp->mptr;
    iput(mip);
    free(oftp);
    return 0;
}

int file_read(int fd, char *buf, int nbytes)
{
    if (fd < 0 || fd > 9)
    {
        printf("ERROR: INVALID FILE DESCRIPTOR\n");
        return -1;
    }
    if (running->fd[fd] == NULL)
    {
        printf("ERROR: FILE NOT OPEN\n");
        return -1;
    }
    if(running->fd[fd]->mode != 0 && running->fd[fd]->mode != 2)
    {
        printf("ERROR: NO READ ACCESS\n");
        return -1;
    }
    int avail, lblk, lblk2, lblk3, blk, startByte, remain, count = 0;
    char *cq, *cp, readBuf[BLKSIZE];
    int tempBuf1[256], tempBuf2[256];
    int size = running->fd[fd]->mptr->inode.i_size;
    avail = size - running->fd[fd]->offset;
    cq = buf;
    while (nbytes && avail)
    {
        lblk = running->fd[fd]->offset / BLKSIZE;
        startByte = running->fd[fd]->offset % BLKSIZE;
        if(lblk <12)
        {
            blk = running->fd[fd]->mptr->inode.i_block[lblk];
        }
        else if (lblk >= 12 && lblk < 256 + 12)
        {
            get_block(running->fd[fd]->mptr->dev, running->fd[fd]->mptr->inode.i_block[12], (char*)tempBuf1);
            blk = tempBuf1[lblk-12];
        }
        else
        {
            get_block(running->fd[fd]->mptr->dev, running->fd[fd]->mptr->inode.i_block[13], (char*)tempBuf1);
            lblk2 = (lblk - (256+12)) / 256;
            lblk3 = (lblk - (256+12)) % 256;
            get_block(running->fd[fd]->mptr->dev, tempBuf1[lblk2], (char*)tempBuf2);
            blk = tempBuf2[lblk3];
        }
        get_block(running->fd[fd]->mptr->dev, blk, readBuf);
        cp = readBuf + startByte;
        remain = BLKSIZE - startByte;
        if(avail >= BLKSIZE && remain == BLKSIZE && nbytes >= BLKSIZE) //Copy the entire block
        {
            strncpy(cq, cp, BLKSIZE);
            running->fd[fd]->offset += BLKSIZE;
            count += BLKSIZE; avail -= BLKSIZE; nbytes -= BLKSIZE; remain -= BLKSIZE;
        }
        else if (nbytes <= avail && nbytes <= remain) //Copy nbytes
        {
            strncpy(cq, cp, nbytes);
            running->fd[fd]->offset += nbytes;
            count += nbytes; avail -= nbytes; nbytes -= nbytes; remain -= nbytes;
        }
        else if (remain <= avail && remain <= nbytes) //Copy remain
        {
            strncpy(cq, cp, remain);
            running->fd[fd]->offset += remain;
            count += remain; avail -= remain; nbytes -= remain; remain -= remain;
        }
        else //Copy avail
        {
            strncpy(cq, cp, avail);
            running->fd[fd]->offset += avail;
            count += avail; avail -= avail; nbytes -= avail; remain -= avail;
        }
    }
    printf("******myread: read %d chars from file %d******\n", count, fd);
    return count;
}

int file_write(int fd, char *buf, int nbytes)
{
    if(fd < 0 || fd > 9)
    {
        printf("ERROR: INVALIDE FILE DESCRIPTOR\n");
        return -1;
    }
    if(running->fd[fd] == NULL)
    {
        printf("ERROR: FILE NOT OPEN\n");
        return -1;
    }
    if(running->fd[fd]->mode == 0)
    {
        printf("ERROR: FILE OPEN FOR READ ONLY\n");
        return -1;
    }

    int blk, newblk, lblk, lblk2, lblk3, startByte, remain, nBytesTot;
    int tempBuf1[256], tempBuf2[256];
    char writeBuf[BLKSIZE];
    char *cp, *cq;
    cq = buf;
    nBytesTot = nbytes;
    while (nbytes > 0)
    {
        lblk = running->fd[fd]->offset / BLKSIZE;
        startByte = running->fd[fd]->offset % BLKSIZE;
        if(lblk <12)
        {
            blk = running->fd[fd]->mptr->inode.i_block[lblk];
            if(blk == 0)
            {
                blk = balloc(running->fd[fd]->mptr->dev);
                running->fd[fd]->mptr->inode.i_block[lblk] = blk;
            }
        }
        else if (lblk >= 12 && lblk < 256 + 12)
        {
            if(running->fd[fd]->mptr->inode.i_block[12] == 0)
            {
                newblk = balloc(running->fd[fd]->mptr->dev);
                running->fd[fd]->mptr->inode.i_block[12] = newblk;
            }
            memset((char*)tempBuf1, 0, BLKSIZE);
            get_block(running->fd[fd]->mptr->dev, running->fd[fd]->mptr->inode.i_block[12], (char*)tempBuf1);
            blk = tempBuf1[lblk-12];
            if (blk == 0)
            {
                blk = balloc(running->fd[fd]->mptr->dev);
                tempBuf1[lblk-12] = blk;
                put_block(running->fd[fd]->mptr->dev, running->fd[fd]->mptr->inode.i_block[12], (char*)tempBuf1);
            }
        }
        else
        {
            if(running->fd[fd]->mptr->inode.i_block[13] == 0)
            {
                newblk = balloc(running->fd[fd]->mptr->dev);
                running->fd[fd]->mptr->inode.i_block[13] = newblk;
            }
            memset((char*)tempBuf1, 0, BLKSIZE);
            get_block(running->fd[fd]->mptr->dev, running->fd[fd]->mptr->inode.i_block[13], (char*)tempBuf1);
            lblk2 = (lblk - (256+12)) / 256;
            lblk3 = (lblk - (256+12)) % 256;
            if(tempBuf1[lblk2] == 0)
            {
                newblk = balloc(running->fd[fd]->mptr->dev);
                tempBuf1[lblk2] = newblk;
            }
            memset((char*)tempBuf2, 0, BLKSIZE);
            get_block(running->fd[fd]->mptr->dev, tempBuf1[lblk2], (char*)tempBuf2);
            blk = tempBuf2[lblk3];
            if(blk == 0)
            {
                blk = balloc(running->fd[fd]->mptr->dev);
                tempBuf2[lblk3] = blk;
                put_block(running->fd[fd]->mptr->dev, tempBuf1[lblk2], (char*)tempBuf2);
            }
            put_block(running->fd[fd]->mptr->dev, running->fd[fd]->mptr->inode.i_block[13], (char*)tempBuf1);
        }
        memset(writeBuf, 0, BLKSIZE);
        get_block(running->fd[fd]->mptr->dev, blk, writeBuf);
        cp = writeBuf + startByte;
        remain = BLKSIZE - startByte;
        if (remain < nbytes) //Copy remain
        {
            strncpy(cp, cq, remain);
            nbytes -= remain; running->fd[fd]->offset += remain;
            if(running->fd[fd]->offset > running->fd[fd]->mptr->inode.i_size)
            {
                running->fd[fd]->mptr->inode.i_size += remain;
            }
            remain -= remain;
        }
        else //Copy nbytes
        {
            strncpy(cp, cq, nbytes);
            remain -= nbytes; running->fd[fd]->offset += nbytes;
            if(running->fd[fd]->offset > running->fd[fd]->mptr->inode.i_size)
            {
                running->fd[fd]->mptr->inode.i_size += nbytes;
            }
            nbytes -= nbytes;
        }
        put_block(running->fd[fd]->mptr->dev, blk, writeBuf);
    }
    running->fd[fd]->mptr->dirty = 1;
    printf("******wrote %d chars into file fd = %d******\n", nBytesTot, fd);
    return nBytesTot;
}

int file_lseek(int fd, int position)
{
    OFT *oftp;
    int original;
    if (fd > 9 || fd < 0)
    {
        printf("ERROR: FILE DESCRIPTOR OUT OF RANGE\n");
        return -1;
    }
    if(running->fd[fd] == NULL)
    {
        printf("ERROR: FILE DESCRIPTOR NOT FOUND\n");
        return -1;
    }
    oftp = running->fd[fd];
    original = oftp->offset;
    if(position < 0)
    {
        oftp->offset = 0;
        return original;
    }
    if(position > (oftp->mptr->inode.i_size - 1))
    {
        oftp->offset = oftp->mptr->inode.i_size - 1;
        return original;
    }
    oftp->offset = position;
    return original;
}

int file_cat(char *pathname)
{
    char mybuf[1024];
    int fd, n = 0;
    fd = file_open(pathname,0);
    if(fd < 0 || fd > 9) { return -1; }
    while( n = file_read(fd, mybuf, 1024))
    {
        mybuf[n] = 0;
        printf("%s", mybuf);
    }

    file_close(fd);
    return 0;
}

int file_cp(char *oldfile,char *newfile)
{
    int fd, gd, n;
    char buf[BLKSIZE], src[256], dest[256], origDest[256];

    if(newfile == NULL || oldfile==NULL) {return -1;}
    fd = file_open(oldfile,0);
    if(fd < 0 || fd > 9){ return -1;}
    gd = file_open(newfile,1);
    if(gd < 0 || gd > 9)
    {
        if(create(newfile) <= 0) {file_close(fd); return -1;}
        gd = file_open(newfile,1);
        if(gd < 0 || gd > 9) {file_close(fd); return -1;}
    }
    while(n = file_read(fd, buf, 1024))
    {
        file_write(gd, buf, n);
    }
    file_close(fd);
    file_close(gd);
    return 0;
}
