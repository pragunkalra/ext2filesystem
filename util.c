/*********** util.c file ****************/
extern int  dev;
extern char gpath[128];
extern char *name[32];
extern int n;
extern PROC *running;
extern MINODE minode[NMINODE];
extern MINODE *root;
extern int inode_start;


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

int tokenize(char *pathname)
{
  int i;
  char *s;
  printf("tokenize %s\n", pathname);

  strcpy(gpath, pathname);   // tokens are in global gpath[ ]
  n = 0;

  s = strtok(gpath, "/");
  while(s){
    name[n] = s;
    n++;
    s = strtok(0, "/");
  }

  for (i= 0; i<n; i++)
    printf("%s  ", name[i]);
  printf("\n");
}

// return minode pointer to loaded INODE
MINODE *iget(int dev, int ino)
{
  int i;
  MINODE *mip;
  char buf[BLKSIZE];
  int blk, offset;
  INODE *ip;

  for (i=0; i<NMINODE; i++){
    mip = &minode[i];
    if (mip->dev == dev && mip->ino == ino){
       mip->refCount++;
       //printf("found [%d %d] as minode[%d] in core\n", dev, ino, i);
       return mip;
    }
  }
    
  for (i=0; i<NMINODE; i++){
    mip = &minode[i];
    if (mip->refCount == 0){
       //printf("allocating NEW minode[%d] for [%d %d]\n", i, dev, ino);
       mip->refCount = 1;
       mip->dev = dev;
       mip->ino = ino;

       // get INODE of ino into buf[ ]    
       blk    = (ino-1)/8 + inode_start;
       offset = (ino-1) % 8;

       //printf("iget: ino=%d blk=%d offset=%d\n", ino, blk, offset);

       get_block(dev, blk, buf);
       ip = (INODE *)buf + offset;
       // copy INODE to mp->INODE
       mip->INODE = *ip;
       return mip;
    }
  }   
  printf("PANIC: no more free minodes\n");
  return 0;
}

void iput(MINODE *mip)
{
 int ino, block, offset;
 char buf[BLKSIZE];
 INODE *ip;

 mip->refCount--;

 if (mip->refCount > 0)  // minode is still in use
    return;
 if (!mip->dirty)        // INODE has not changed; no need to write back
    return;

 ino = mip->ino;
 block = (ino-1) / 8 + inode_start;
 offset = (ino-1) % 8;
 get_block(mip->dev, block, buf);

 ip = (INODE*)buf + offset;
 memcpy(ip, &mip->INODE, sizeof(INODE));

 put_block(mip->dev, block, buf);

 //Modifications have been saved, so no longer dirty
 mip->dirty = 0;
 
} 

int search(MINODE *mip, char *name)
{
   char *cp, c, sbuf[BLKSIZE], temp[256];
   DIR *dp;
   INODE *ip;

   printf("search for %s in MINODE = [%d, %d]\n", name, mip->dev, mip->ino);
   ip = &(mip->INODE);

   /*** search for name in mip's data blocks: ASSUME i_block[0] ONLY ***/

   get_block(dev, ip->i_block[0], sbuf);
   dp = (DIR *)sbuf;
   cp = sbuf;
   printf("  ino   rlen  nlen  name\n");

   while (cp < sbuf + BLKSIZE){
     strncpy(temp, dp->name, dp->name_len);
     temp[dp->name_len] = 0;
     printf("%4d  %4d  %4d    %s\n", 
           dp->inode, dp->rec_len, dp->name_len, temp);
     if (strcmp(temp, name)==0){
        printf("found %s : ino = %d\n", temp, dp->inode);
        return dp->inode;
     }
     cp += dp->rec_len;
     dp = (DIR *)cp;
   }
   return 0;
}

int getino(char *pathname)
{
  int i, j, ino, blk, disp, pino;
  char buf[BLKSIZE], temp[BLKSIZE], child[BLKSIZE], parent[BLKSIZE], *cp;
  INODE *ip;
  MINODE *mip, *newmip;
  DIR *dp;

  printf("getino: pathname=%s\n", pathname);
  if (strcmp(pathname, "/")==0)
      return 2;
  
  // starting mip = root OR CWD
  if (pathname[0]=='/')
     mip = root;
  else
     mip = running->cwd;

  dev = mip->dev;

  mip->refCount++;         // because we iput(mip) later
  
  tokenize(pathname);

   strcpy(buf, pathname);

   strcpy(temp, buf);
   strcpy(parent, dirname(temp)); // dirname destroys path

   strcpy(temp, buf);
   strcpy(child, basename(temp)); // basename destroys path


  for (i=0; i<n; i++){
     if((strcmp(name[i], "..")) == 0 && (mip->dev != root->dev) && (mip->ino == 2))
     {
        printf("UP cross mounting point\n");

        // Find entry in mount table
        for(j = 0; j < MT_SIZE; j++)
        {
           if(mtable[j].dev  == dev)
           {
              break;
           }
        }
        newmip = mtable[j].mntDirPtr;
        iput(mip);
      
        get_block(newmip->dev, newmip->INODE.i_block[0], buf); //First block contains . and .. entries
        dp = (DIR*)buf;
        cp = (char *)buf;
        strncpy(temp, dp->name, dp->name_len);
        temp[dp->name_len] = 0; // add null terminating character
        while(strcmp(temp, ".."))
        {
           if((cp + dp->rec_len) >= (buf + BLKSIZE))
           {
              printf("Can't find partent directory\n");
              return 0;
           }
           cp += dp->rec_len;
           dp = (DIR *)cp;
           strncpy(temp, dp->name, dp->name_len);
           temp[dp->name_len] = 0; // add null terminating character
        }
        
        mip = iget(dev, dp->inode);

        dev = newmip->dev;
        continue;
     }
      printf("===========================================\n");
      printf("getino: i=%d name[%d]=%s\n", i, i, name[i]);
      
      if(!my_maccess(mip, 'x'))
      {
         printf(" -> ACCESS DENIED\n");
         iput(mip);
         return 0;
      }

      ino = search(mip, name[i]);

      if (ino==0){
         iput(mip);
         printf("name %s does not exist\n", name[i]);
         return 0;
      }
      iput(mip);                // release current mip
      mip = iget(dev, ino);     // get next mip

      if(mip->mounted)
      {
         printf("DOWN cross mounting point\n");
         iput(mip);

         dev = mip->mptr->dev;
         mip = iget(dev, 2);
      }
   }

  iput(mip);                   // release mip  
   return mip->ino;
}


MINODE * iget_mp(char *pathname)
{
  int i, j, ino, blk, disp, pino;
  char buf[BLKSIZE], temp[BLKSIZE], child[BLKSIZE], parent[BLKSIZE], *cp;
  INODE *ip;
  MINODE *mip, *newmip;
  DIR *dp;

  printf("getino: pathname=%s\n", pathname);
  if (strcmp(pathname, "/")==0)
      return root;
  
  // starting mip = root OR CWD
  if (pathname[0]=='/')
     mip = root;
  else
     mip = running->cwd;

  dev = mip->dev;

  mip->refCount++;         // because we iput(mip) later
  
  tokenize(pathname);

   strcpy(buf, pathname);

   strcpy(temp, buf);
   strcpy(parent, dirname(temp)); // dirname destroys path

   strcpy(temp, buf);
   strcpy(child, basename(temp)); // basename destroys path


  for (i=0; i<n; i++){
     if((strcmp(name[i], "..")) == 0 && (mip->dev != root->dev) && (mip->ino == 2))
     {
        printf("UP cross mounting point\n");

        // Find entry in mount table
        for(j = 0; j < MT_SIZE; j++)
        {
           if(mtable[j].dev  == dev)
           {
              break;
           }
        }
        newmip = mtable[j].mntDirPtr;
        iput(mip);
      
        get_block(newmip->dev, newmip->INODE.i_block[0], buf); //First block contains . and .. entries
        dp = (DIR*)buf;
        cp = (char *)buf;
        strncpy(temp, dp->name, dp->name_len);
        temp[dp->name_len] = 0; // add null terminating character
        while(strcmp(temp, ".."))
        {
           if((cp + dp->rec_len) >= (buf + BLKSIZE))
           {
              printf("Can't find partent directory\n");
              return 0;
           }
           cp += dp->rec_len;
           dp = (DIR *)cp;
           strncpy(temp, dp->name, dp->name_len);
           temp[dp->name_len] = 0; // add null terminating character
        }
        
        mip = iget(dev, dp->inode);

        dev = newmip->dev;
     }
      printf("===========================================\n");
      printf("getino: i=%d name[%d]=%s\n", i, i, name[i]);
      
      if(!my_maccess(mip, 'x'))
      {
         printf(" -> ACCESS DENIED\n");
         iput(mip);
         return 0;
      }

      ino = search(mip, name[i]);

      if (ino==0){
         iput(mip);
         printf("name %s does not exist\n", name[i]);
         return 0;
      }
      iput(mip);                // release current mip
      mip = iget(dev, ino);     // get next mip

      if(mip->mounted && i < n-1)
      {
         printf("DOWN cross mounting point\n");
         iput(mip);

         dev = mip->mptr->dev;
         mip = iget(dev, 2);
      }
   }

   iput(mip);                   // release mip  
   return mip;
}


int findmyname(MINODE *parent, u32 myino, char *myname) 
{
   INODE * IP;
   DIR * dp;
   char * cp, buf[BLKSIZE], temp[256];


   ip = &(parent->INODE);
   
   if(!S_ISDIR(ip->i_mode))
   {
      printf("Not a directory\n");
      return 1;
   }

   //assume only 12 blocks
   for(int i = 0; i<12; i++){
      if(ip->i_block[i]){
         get_block(parent->dev, ip->i_block[i], buf);
         dp = (DIR *)buf;
         cp = buf;
         while(cp < buf + BLKSIZE){
            if(myino == dp->inode){
               strncpy(myname, dp->name, dp->name_len);
               myname[dp->name_len] = 0;
               return 0;
            }
            cp += dp->rec_len;
            dp = (DIR *)cp;
         }
      }
   }
   //not found
   return -1;

}

int findino(MINODE *mip, u32 *myino) // myino = ino of . return ino of ..
{
  char buf[BLKSIZE], *cp;   
  DIR *dp;

  get_block(mip->dev, mip->INODE.i_block[0], buf);
  cp = buf; 
  dp = (DIR *)buf;
  *myino = dp->inode;
  cp += dp->rec_len;
  dp = (DIR *)cp;
  return dp->inode;
}

int my_truncate(MINODE * mip)
{
   //Set to 1 just to get into while loop
   int bno = 1, lbk = 0, iblk, diblk;

   int buf[BLKSIZE/sizeof(int)], buf2[BLKSIZE/sizeof(int)];


   while(bno)
   {
      if(lbk < 12){
         bno = mip->INODE.i_block[lbk];
      }
      else if(lbk >= 12 && lbk < 12 + 256){
         if(!mip->INODE.i_block[12]) break;
         get_block(mip->dev, mip->INODE.i_block[12], (char *)buf);
         bno = buf[lbk-12];
      }
      else if(lbk >= 12 + 256 && lbk < 12 + 256 + 256*256)
      {
         if(!mip->INODE.i_block[13]) break;
         get_block(mip->dev, mip->INODE.i_block[13], (char *)buf);
         iblk = (lbk - (12 + 256)) / 256;
         if(!buf[iblk]) break;
         get_block(mip->dev, buf[iblk], (char *)buf2);
         diblk = (lbk - (12 + 256)) % 256;
         bno = buf2[diblk];
      }

      if(!bno) break;
      bdalloc(mip->dev, bno);
      lbk++;
   }
   mip->INODE.i_mtime = mip->INODE.i_atime = time(NULL);
   mip->INODE.i_size = 0;
   mip->dirty = 1;
}



int my_access(char *pathname, char mode) // mode ='r', 'w', 'x' 
{
   unsigned short check_mode;
   int ino = getino(pathname);
   if(!ino){
      printf(" -> File does not exist\n");
      return 0;
   }

   if (running->uid == SUPER_USER) return 1;

   MINODE * mip = iget(dev, ino);

   switch(mode){
      case 'r':
         check_mode = 1 << 2;
         break;
      case 'w':
         check_mode = 1 << 1;
         break;
      case 'x':
         check_mode = 1;
         break;
      default:
         printf(" -> Invalid mode\n");
         return 0;
   }

   //Check for user permission if user
   if(mip->INODE.i_uid == running->uid && (mip->INODE.i_mode & (check_mode << 6))){
      iput(mip);
      return 1;
   }

   //Check for group permission if in group
   if(mip->INODE.i_gid == running->gid && (mip->INODE.i_mode & (check_mode << 3))){
      iput(mip);
      return 1;
   }

   //Check for other permission
   if(mip->INODE.i_mode & check_mode){
      iput(mip);
      return 1;
   }

   iput(mip);
   return 0;
}


int my_maccess(MINODE *mip, char mode) // mode ='r', 'w', 'x' 
{  
   unsigned short check_mode;

   if (running->uid == SUPER_USER) return 1;

   mip->refCount++;

   switch(mode){
      case 'r':
         check_mode = 1 << 2;
         break;
      case 'w':
         check_mode = 1 << 1;
         break;
      case 'x':
         check_mode = 1;
         break;
      default:
         printf(" -> Invalid mode\n");
         return 0;
   }

   //Check for user permission if user
   if(mip->INODE.i_uid == running->uid && (mip->INODE.i_mode & (check_mode << 6))){
      iput(mip);
      return 1;
   }

   //Check for group permission if in group
   if(mip->INODE.i_gid == running->gid && (mip->INODE.i_mode & (check_mode << 3))){
      iput(mip);
      return 1;
   }

   //Check for other permission
   if(mip->INODE.i_mode & check_mode){
      iput(mip);
      return 1;
   }

   iput(mip);
   return 0;
}


void my_sw(void)
{
   running->status = FREE;
   running = running->next;
   running->status = READY;
   printf(  "Process %d is now active\n", running->pid);
}
