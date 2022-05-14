/************* open_close.c file **************/


#define MODE_R          0
#define MODE_W          1
#define MODE_RW         2
#define MODE_APPEND     3


int open_file(char *path, int mode)
{
    if(mode > 3 || mode < 0)
    {
        printf(" -> Invalid mode\n");
        return -1;
    }

    if(path[0] == '/') dev = root->dev;
    else dev = running->cwd->dev;

    int ino = getino(path);

    //If file doesn't exist and are opening in write mode, create it
    if(ino == 0)
    {   
        if(mode == MODE_R)
        {
            printf(" -> File doesn't exist\n");
            return -2;
        }
        create_file(path);
        ino = getino(path);
        if(!ino)
        {
            printf(" -> File doesn't exist and could not be created\n");
            return -3;
        }
    }

    MINODE *mip = iget(dev, ino);

    if(!S_ISREG(mip->INODE.i_mode))
    {
        printf(" -> Not a regular file\n");
        iput(mip);
        return -4;
    }


    //Check for read permission if necessary
    if(mode == MODE_R || mode == MODE_RW)
    {
        if(!my_maccess(mip, 'r'))
        {
            printf(" -> You do not have permission to do this\n");
            iput(mip);
            return -5;
        }
    }

    //Check for write permission if necessary
    if(mode == MODE_W || mode == MODE_APPEND || mode == MODE_RW)
    {
        if(!my_maccess(mip, 'w'))
        {
            printf(" -> You do not have permission to do this\n");
            iput(mip);
            return -6;
        }
    }


    OFT * findOFT = NULL;
    for(int i = 0; i < NOFT; i++)
    {
        if(oft[i].refCount == 0)
        {
            if(!findOFT) findOFT = &oft[i];
        }
        else if(oft[i].mptr == mip)
        {
            //If already open and not in read mode
            if(oft[i].mode != MODE_R){
                printf(" -> File is currently being accessed\n");
                iput(mip);
                return -7;
            }
            //If already open in read mode, but trying to open in a different mode
            else if(mode != MODE_R){
                printf(" -> File is currently being accessed\n");
                iput(mip);
                return -8;
            }
            //Already open in read mode, and trying to open in Read mode, so OK
            else{
                findOFT = &oft[i];
                findOFT->refCount++;
                break;
            }
        }
    }

    if(!findOFT)
    {
        printf(" -> No available OFT found\n");
        iput(mip);
        return -9; 
    }

    //This means we didn't find existing OFT, so initialize it
    if(findOFT->refCount == 0)
    {
        findOFT->refCount = 1;
        findOFT->offset = (mode == MODE_APPEND) ? mip->INODE.i_size : 0;
        findOFT->mode = mode;
        findOFT->mptr = mip;
        if(mode == MODE_W) my_truncate(mip);
    }

    //Find first open FD
    int j;
    for(j = 0; j<NFD; j++)
    {
        if(!running->fd[j])
        {
            running->fd[j] = findOFT;
            break;
        }
    }

    //If nowhere to put, show error, decrement ref count, and release minode
    if(j == NFD){
        printf(" -> No available FD's found\n");
        findOFT->refCount--;
        iput(mip);
        return -10;
    }

    //update access time
    mip->INODE.i_atime = time(NULL);

    //update modified time
    if(mode != MODE_R)  mip->INODE.i_mtime = mip->INODE.i_atime;

    mip->dirty = 1;
    //return FD
    return j;
}


int close_file(int fd)
{
    if(fd < 0 || fd >= NFD){
        printf(" -> Invalid file descriptor\n");
        return -1;
    }

    if(!running->fd[fd]){
        printf(" -> File descriptor does not exist\n");
        return -2;
    }

    OFT *oftp = running->fd[fd];
    running->fd[fd] = NULL;
    oftp->refCount--;

    //If file is accessed somewhere else, leave it
    if(oftp->refCount > 0) return 0;

    //If file no longer accessed by anything, free minode
    iput(oftp->mptr);
    oftp->mptr = NULL;
    oftp->offset = 0;
    return 0;
}


int my_lseek(int fd, int position)
{
    int initial_offset;
    OFT *oftp = running->fd[fd];

    if(!oftp){
        printf(" -> fd does not exist\n");
        return -1;
    }

    if(position > oftp->mptr->INODE.i_size || position < 0){
        printf(" -> position is not within file bounds\n");
        return -2;
    }

    initial_offset = oftp->offset;
    oftp->offset = position;
    return initial_offset;
}


int my_pfd(void)
{
    char * modes[] = {"READ", "WRITE", "READ/WRITE", "APPEND"};
    printf(" fd      mode     offset     INODE\n");
    printf("----  ----------  ------  -----------\n");
    for( int i = 0; i < NFD; i++)
    {
        if(running->fd[i]){
            printf("%-4d  %-10s  %-6d  [%-4d,%-4d]\n", i, modes[running->fd[i]->mode], running->fd[i]->offset, running->fd[i]->mptr->dev, running->fd[i]->mptr->ino);
        }
    }
    return 0;
}
