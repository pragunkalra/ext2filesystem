/************* write_cp.c file **************/


int my_write(int fd, char buf[], int nbytes)
{
    MINODE *mip;
    OFT *oftp;

    int avil, blk, lbk, dblk, startByte;

    char writebuf[BLKSIZE], tempbuf[BLKSIZE];
    int buf_12[256], buf_13[256], dbuf[256];

    char *cq, *cp;
    cq = buf;

    oftp = running->fd[fd];
    mip = oftp->mptr;


    lbk = oftp->offset / BLKSIZE;
    startByte = oftp->offset % BLKSIZE;

    // DIRECT
    if (lbk < 12)
    {
        if(mip->INODE.i_block[lbk] == 0)
        {
            mip->INODE.i_block[lbk] = balloc(mip->dev);
        }
        blk = mip->INODE.i_block[lbk];
    }
    // INDIRECT
    else if (lbk >= 12 && lbk < (256+12))
    {
        // if free, allocate new block
        if (mip->INODE.i_block[12] == 0)
        {
            mip->INODE.i_block[12] = balloc(mip->dev);
        }
        
        lbk -= 12;
        get_block(mip->dev, mip->INODE.i_block[12], (char *)buf_12);

        blk = buf_12[lbk];
        // allocate new block if does not exist
        if(blk == 0)
        {
            // allocate new block
            blk = buf_12[lbk] = balloc(mip->dev);
            // record block
            put_block(mip->dev, mip->INODE.i_block[12], (char *)buf_12);
        }
    }
    // DOUBLE INDIRECT
    else
    {
        if(mip->INODE.i_block[13] == 0)
        {
            mip->INODE.i_block[13] = balloc(mip->dev);
        }

        lbk -= (12 + 256);

        get_block(mip->dev, mip->INODE.i_block[13], (char *)buf_13);

        dblk = buf_13[lbk/256];

        // allocate new block if does not exist
        if(dblk == 0)
        {
            dblk = buf_13[lbk/256] = balloc(mip->dev);
            put_block(mip->dev, mip->INODE.i_block[13], (char *)buf_13);
        }

        get_block(mip->dev, dblk, (char *)dbuf);

        blk = dbuf[lbk%256];

        // allocate new block if does not exist
        if(blk == 0)
        {
            blk = dbuf[lbk%256] = balloc(mip->dev);
            put_block(mip->dev, dblk, (char *)dbuf);
        }

    }

    // get data block into readbuf
    get_block(mip->dev, blk, writebuf);

    cp = writebuf + startByte;

    strcpy(cp, cq);
    oftp->offset+=nbytes;
    if(oftp->offset > mip->INODE.i_size)
    {
        mip->INODE.i_size+=nbytes;
    }

    put_block(mip->dev, blk, writebuf);

    mip->dirty = 1;
    printf("wrote %d bytes into file descriptor fd=%d\n", nbytes, fd);

    return nbytes;
}


int write_file(int fd, char *str)
{
    char buf[256];

    if(fd < 0 || fd > NFD)
    {
        printf("File descriptor provided is not valid.");
        return -1;
    }

    strcpy(buf, str);

    return my_write(fd, buf, strlen(buf));
}


int my_cp(char *src, char*dest)
{
    char destination[256], sourceFileName[256];

    strcpy(destination, dest);
    strcpy(sourceFileName, src);
    strcpy(sourceFileName, basename(sourceFileName));

    if(!strcmp(src, "") || !strcmp(dest, "")){
        printf(" -> Either the source or destination file was not specified\n");
        return -1;
    }

    int ino = getino(src);
    if(!ino){
        printf(" -> Source file does not exist\n");
        return -2;
    }

    if(dest[0] == '/') dev = root->dev;
    else dev = running->cwd->dev;

    //If inode exists, check if directory, otherwise file will be created in file_open()
    if(ino = getino(dest))
    {
        //Get dest in minode so we can check if it's a directory
        MINODE *mip = iget(dev, ino);

        //If a directory, add src file name to destination path
        if(S_ISDIR(mip->INODE.i_mode))
        {
            strcat(destination, "/");
            strcat(destination, sourceFileName);
        }
        iput(mip);
    }

    int fd, gd, n;
    char buf[BLKSIZE];
    fd = open_file(src, MODE_R);
    if(fd < 0){
        //open_file will print the error
        return -3;
    }

    //If destination file doesn't exist already, open_file will create it (for all write modes)
    gd = open_file(destination, MODE_RW);

    while(n = my_read(fd, buf, BLKSIZE, 1))
    {
        my_write(gd, buf, n);
    }

    close_file(fd);
    close_file(gd);
    return 0;
}
