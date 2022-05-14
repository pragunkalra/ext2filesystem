/************* mkdir.c file **************/
extern char gpath[128];
extern int n, dev, ninodes, nblocks, imap, bmap;
extern PROC *running;

int enter_name(MINODE *pmip, int myino, char *myname){
  char buf[BLKSIZE], *cp;
  DIR *dp;

  INODE *iparent = &pmip->INODE; // get parent inode

  int pblk = 0, remain = 0;
  int ideal_length = 0, need_length = 0, i;

  for(i = 0; i < (iparent->i_size / BLKSIZE); i++)
  {
    if(iparent->i_block[i]==0)
      break;

    pblk = iparent->i_block[i]; // get current block number

    need_length = 4 * ((8 + strlen(myname) + 3)/4); // needed length of new dir name in bytes

    printf("ideal = %d\nneed = %d\n", ideal_length, need_length);

    get_block(pmip->dev, pblk, buf); // get current parent inode block

    dp = (DIR*)buf; // cast current inode block as directory pointer
    cp = buf;

    printf("Entering last entry in data block %d\n", pblk);

    while((cp + dp->rec_len) < (buf + BLKSIZE))
    {
      cp += dp->rec_len;
      dp = (DIR *)cp; // dp now points at last entry in block
    }

    ideal_length = 4 * ((8 + dp->name_len + 3)/4); // length until next directory entry

    cp = (char*)dp;

    remain = dp->rec_len - ideal_length; // remaining length
    printf("remain = %d\n", remain);

    if(remain >= need_length)
    {
      dp->rec_len = ideal_length;

      cp += dp->rec_len; // set cp to end of ideal
      dp = (DIR*)cp; // end of last entry

      dp->inode = myino; // set end of entry to provided inode
      dp->rec_len = BLKSIZE - ((uintptr_t)cp - (uintptr_t)buf);
      dp->name_len = strlen(myname);
      dp->file_type = EXT2_FT_DIR;

      strcpy(dp->name, myname);

      put_block(pmip->dev, pblk, buf); // write block back

      return 0;
    }
  }
    //If we get here, there's not enough space in the allocated blocks, so allocate another
    printf("Block number = %d\n", i);
    
    pblk = balloc(pmip->dev); // get the first available block for new inode

    iparent->i_block[i] = pblk;

    iparent->i_size += BLKSIZE;
    pmip->dirty = 1;

    get_block(pmip->dev, pblk, buf);

    dp = (DIR*)buf;
    cp = buf;

    printf("Directory Name = %s\n", dp->name);

    dp->inode = myino;
    dp->rec_len = BLKSIZE;
    dp->name_len = strlen(myname);
    dp->file_type = EXT2_FT_DIR;

    strcpy(dp->name, myname);

    put_block(pmip->dev, pblk, buf); // write block

    return 0;
}


int my_mkdir(MINODE *pip, char *child)
{
  DIR *dp;
  char buf[BLKSIZE];
  int ino = ialloc(pip->dev);
  int bno = balloc(pip->dev);

  MINODE *mip = iget(pip->dev,ino);
  INODE *ip = &mip->INODE;

  ip->i_mode = 0x41ED;		// OR 040755: DIR type and permissions
  ip->i_uid  = running->uid;	// Owner uid 
  ip->i_gid  = running->gid;	// Group Id
  ip->i_size = BLKSIZE;		// Size in bytes 
  ip->i_links_count = 2;	        // Links count=2 because of . and ..
  ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);  // set to current time
  ip->i_blocks = 2;                	// LINUX: Blocks count in 512-byte chunks 
  ip->i_block[0] = bno;             // new DIR has one data block
  for(int i = 1; i<15;i++)
  {
    ip->i_block[i] = 0;
  }

  mip->dirty = 1;               // mark minode dirty
  iput(mip);                    // write INODE to disk

  //DIR created, time to add . and .. entries to allocated block

  get_block(pip->dev, bno, buf);//Read block from disk
  dp = (DIR*)buf;

  dp->inode = ino;
  dp->name[0] = '.';
  dp->name_len = strlen(".");
  dp->rec_len = 12;

  dp = (DIR *)((char *)dp + dp->rec_len);//Move to next entry

  dp->inode = pip->ino;
  dp->name[0] = dp->name[1] = '.';
  dp->name_len = strlen("..");
  dp->rec_len = 1012;//Rest of block

  put_block(pip->dev, bno, buf);//Write block back to disk
  enter_name(pip, ino, child);//enter name into parent's directory
  
  return 0;
}


int make_dir(char *path) 
{ 
  char buf[128], parent[128], child[128], temp[128];
  MINODE *pmip; 


  strcpy(buf, path);

  strcpy(temp, buf);
  strcpy(parent, dirname(temp)); // dirname destroys path

  strcpy(temp, buf);
  strcpy(child, basename(temp)); // basename destroys path

  int pino;

  pino = getino(parent);
  if(pino < 1)
  {
    printf(" -> Specified parent directory does not exits");
    return -1;
  }

  pmip = iget(dev, pino);

  if(!my_maccess(pmip, 'w'))
  {
    printf(" -> You do not have write permission in this directory\n");
    iput(pmip);
    return -2;
  }

  if(!S_ISDIR(pmip->INODE.i_mode))
  {
    printf(" -> Filepath does not point to a directory\n");
    iput(pmip);
    return -2;
  }

  if(getino(path)){
    printf(" -> Directory already exists\n");
    iput(pmip);
    return -3;
  }

  my_mkdir(pmip, child);

  pmip->INODE.i_links_count++;
  pmip->INODE.i_atime = time(NULL);
  pmip->dirty = 1;

  iput(pmip);

  return 0;
}


int my_creat(MINODE *pmip, char *child)
{
  DIR *dp;
  int ino = ialloc(pmip->dev);

  MINODE *mip = iget(pmip->dev,ino);
  INODE *ip = &mip->INODE;

  ip->i_mode = 0x81A4;		// OR 0100644: File type and permissions
  ip->i_uid  = running->uid;	// Owner uid 
  ip->i_gid  = running->gid;	// Group Id
  ip->i_size = 0;
  ip->i_links_count = 1;	        
  ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);  // set to current time
  ip->i_blocks = 0;                	// LINUX: Blocks count in 512-byte chunks
  for(int i = 0; i<15;i++)
  {
    ip->i_block[i] = 0;
  }

  mip->dirty = 1;               // mark minode dirty
  iput(mip);                    // write INODE to disk

  enter_name(pmip, ino, child);
  
  return ino;
}


int create_file(char *path) 
{ 
  char buf[128], parent[128], child[128], temp[128];
  MINODE *pmip; 


  strcpy(buf, path);

  strcpy(temp, buf);
  strcpy(parent, dirname(temp)); // dirname destroys path

  strcpy(temp, buf);
  strcpy(child, basename(temp)); // basename destroys path

  int pino;

  pino = getino(parent);
 
  if(!pino){
    printf(" -> Provided parent directory does exists!\n");
    return -1;
  }

  pmip = iget(dev, pino);

  if(getino(path)){
    printf(" -> File already exists!\n");
    iput(pmip);
    return -2;
  }


  if(!my_maccess(pmip, 'w'))
  {
    printf(" -> You do not have write permission in this directory\n");
    iput(pmip);
    return -3;
  }

  if(!S_ISDIR(pmip->INODE.i_mode))
  {
    printf(" -> Provided parent directory is not a directory!\n");
    iput(pmip);
    return -4;
  }

  my_creat(pmip, child);

  pmip->INODE.i_atime = time(NULL);
  pmip->dirty = 1;

  iput(pmip);

  return 0;
}
