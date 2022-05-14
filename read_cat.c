/************* read_cat.c file **************/

extern PROC *running;

int my_read(int fd, char buf[], int nbytes, int verbose)
{
    MINODE *mip;
    OFT *oftp;

    int min, avil, blk, lbk, dblk, startByte, remain, count = 0;

    char readbuf[BLKSIZE], tempbuf[BLKSIZE];
    int buf_12[256], buf_13[256], dbuf[256];

    char *cq, *cp;
    cq = buf;

    oftp = running->fd[fd];
    mip = oftp->mptr;

    // num of bytes available in the file
    avil = mip->INODE.i_size - oftp->offset; 

    while (nbytes && avil)
    {
        lbk = oftp->offset / BLKSIZE;
        startByte = oftp->offset % BLKSIZE;

        // DIRECT
        if (lbk < 12)
        {
            // map logical lbk to physical blk
            blk = mip->INODE.i_block[lbk];
        }
        // INDIRECT
        else if (lbk >= 12 && lbk < (256+12))
        {
            get_block(mip->dev, mip->INODE.i_block[12], (char *)buf_12);

            lbk -= 12;
            
            blk = buf_12[lbk];
        }
        // DOUBLE INDIRECT
        else
        {
            lbk -= (12 + 256);

            get_block(mip->dev, mip->INODE.i_block[13], (char *)buf_13);

            dblk = buf_13[lbk/256];

            get_block(mip->dev, dblk, (char *)dbuf);

            blk = dbuf[lbk%256];

        }

        // get data block into readbuf
        get_block(mip->dev, blk, readbuf);

        cp = readbuf + startByte;
        remain = BLKSIZE - startByte; // number of bytes that remain in readbuf

        // number of bytes to copy
        min = (avil < remain && avil < nbytes) ? avil : (remain < nbytes) ? remain : nbytes; 

        // copy bytes, adjust offset
        strncpy(cq, cp, min);
        oftp->offset += min;
        count+=min;
        avil-=min;
        nbytes-=min;
        remain-=min; 

    }

    if(verbose > 0 && count > 0)
        printf("READ: read %d bytes from file descriptor %d\n", count, fd);

    return count;
}

int read_file(int fd, int nbytes)
{
    int bytes_read;

    char buf[nbytes];

    if(fd < 0 || fd > NFD)
    {
        printf("File descriptor provided is not valid.");
        return -1;
    }
    
    bytes_read = my_read(fd, buf, nbytes, 1);

    printf("%s\n", buf);

    return bytes_read;
} 


int my_cat(char *path)
{
    char mybuf[BLKSIZE];
    int n, fd;

    fd = open_file(path, MODE_R);
    if(fd < 0)
    {
        printf(" -> Unable to open file\n");
        return -1;
    }

    while(n = my_read(fd, mybuf, BLKSIZE, 0)){
        write(1, mybuf, n);
    }

    close_file(fd);
}
