/************* rmdir.c file **************/


int rm_child(MINODE *pip, char *name){
    
    char buf[BLKSIZE], *cp, temp[128];
    DIR *dp, *dp_prev;;

    for(int i = 0; i < 12 ; i++)
	{   
        //If no more blocks allocated, return
		if(pip->INODE.i_block[i] == 0)
			return -1;

        get_block(pip->dev, pip->INODE.i_block[i], buf);
        cp = buf;
        dp = (DIR*)cp;

        while(cp < buf + BLKSIZE){
            strncpy(temp, dp->name, dp->name_len);
            temp[dp->name_len] = '\0';

            if(!strcmp(name, temp)){ //Found child
                if(cp + dp->rec_len == buf + 1024)//Last entry in block
                {
                    printf("Last entry in block\n");
                    dp_prev->rec_len += dp->rec_len;
                    put_block(dev, pip->INODE.i_block[i], buf);
                }
                
                else if(cp == buf && cp + dp->rec_len == buf + BLKSIZE)//first and only entry
                {
                    printf("First and only entry in block\n");
                    //Only entry in block, so we can get rid of it
                    bdalloc(dev, pip->INODE.i_block[i]);

                    //Move nonzero blocks up to get rid of holes
                    for(; pip->INODE.i_block[i] && i < 11; i++)
                    {
                        get_block(dev, pip->INODE.i_block[i + 1], buf);
                        put_block(dev, pip->INODE.i_block[i], buf);
                    }
                    pip->INODE.i_size -= 1024;
                }
                else//in middle of inodes
                {
                    printf("Middle entry in block\n");
                    char *final_cp = cp + dp->rec_len;
                    DIR *final_dp = (DIR*)final_cp;

                    //Find final entry
                    while(final_cp + final_dp->rec_len < buf + BLKSIZE)
                    {
                        final_cp += final_dp->rec_len;
                        final_dp = (DIR*)final_cp;
                    }
                    //Add rec_len from removed record to last record
                    final_dp->rec_len += dp->rec_len;

                    //Shift remaining records to fill in gap
                    memmove(cp, cp + dp->rec_len, buf + 1024 -(cp + dp->rec_len));
                    put_block(dev, pip->INODE.i_block[i], buf);
                }
                return 0;
            }
            dp_prev = dp;
            cp += dp->rec_len;
            dp = (DIR*)cp;
        }
    }
    return -1;
}


int my_rmdir(char * path){
    if(!strcmp(path, "/" ) || !strcmp(path, ".." ) || !strcmp(path, "." ))
    {
        printf(" -> Cannot delete root directory, current directory, or parent of current directory\n");
        return -1;
    }
    int ino;
    MINODE * mip;

    ino = getino(path);
    if(!ino){
        printf(" -> Directory not found\n");
        return -2;
    }

    mip = iget(dev, ino);

    if(running->uid != mip->INODE.i_uid)
    {
        printf(" -> You do not own this directory\n");
        iput(mip);
        return -3;
    }

    if(mip->refCount > 1)
    {
        printf(" -> MINODE is busy\n");
        iput(mip);
        return -4;
    }

    if(!S_ISDIR(mip->INODE.i_mode)){
        printf(" -> provided path is not a directory\n");
        iput(mip);
        return -5;
    }

    if(mip->INODE.i_links_count > 2){
        printf(" -> Cannot delete a directory that isn't empty\n");
        iput(mip);
        return -6;
    }

    //Dir only has 2 links at this point, but may still contain files
    char buf[BLKSIZE], *cp, filename[64];
	DIR *dp;

    get_block(dev, mip->INODE.i_block[0], buf);
    cp = buf;
    dp = (DIR*)buf;
    while(cp < buf + BLKSIZE){
        strncpy(filename, dp->name, dp->name_len);
        filename[dp->name_len] = '\0';  //Add null termination character
        if(strcmp(filename, ".") && strcmp(filename, "..")){
            iput(mip);
            printf(" -> Cannot delete a directory that isn't empty\n");
            return -7;
        }
        cp += dp->rec_len;
        dp = (DIR *)cp;
    }

    //If we get here, the directory is empty

    for (int i=0; i<12; i++){
        if (mip->INODE.i_block[i]==0)
            break;
        bdalloc(mip->dev, mip->INODE.i_block[i]);
    }
    idalloc(mip->dev, mip->ino);
    iput(mip); //(which clears mip->refCount = 0);


    char parent_path[128], base[128], temp[128];
    strcpy(temp, path);
    strcpy(parent_path, dirname(temp));
    strcpy(temp, path);
    strcpy(base, basename(temp));
    int pino = getino(parent_path);
    MINODE * pip = iget(mip->dev, pino); 

    rm_child(pip, base);

    pip->INODE.i_links_count--;
    pip->INODE.i_atime = pip->INODE.i_mtime = time(NULL);
    pip->dirty = 1;
    iput(pip);

    return 0;
}
