/************* mkdir.c file **************/
extern char gpath[128];
extern int n, dev, ninodes, nblocks, imap, bmap;
extern PROC *running;


int tst_bit(char *buf, int bit) { return buf[bit / 8] & (1 << (bit % 8)); }

int set_bit(char *buf, int bit) { buf[bit / 8] |= (1 << (bit % 8)); }

void clr_bit(char *buf, int bit){buf[bit/8] &= ~(1 << (bit % 8));}

int idalloc(int dev, int ino)  // deallocate an ino number
{ 
  char buf[BLKSIZE];

  if (ino > ninodes || ino < 0){
    printf("inumber %d out of range\n", ino);
    return 0;
  }

  // get inode bitmap block
  get_block(dev, imap, buf);
  clr_bit(buf, ino-1);

  // write buf back
  put_block(dev, imap, buf);
}

int bdalloc(int dev, int blk) // deallocate a blk number
{
    char buf[BLKSIZE];

    if(blk > ninodes || blk < 0){
        printf("bnumber %d out of range\n", blk);
        return 0;
    }

    get_block(dev, bmap, buf);
    clr_bit(buf, blk);

    put_block(dev, bmap, buf);
}

int ialloc(int dev) {
  int i;
  char buf[BLKSIZE];

  get_block(dev, imap, buf);

  for (i = 0; i < ninodes; i++) {
    if (tst_bit(buf, i) == 0) {
      set_bit(buf, i);
      put_block(dev, imap, buf);
      printf("allocated ino = %d\n", i + 1);
      return i + 1;
    }
  }
  printf(" -> inode was not allocated because no free inodes remain\n");
  return 0;
}


int balloc(int dev) {
  char buf[BLKSIZE];

  get_block(dev, bmap, buf);

  for (int i = 0; i < nblocks; i++) {
    if (!tst_bit(buf, i)) {
      set_bit(buf, i);
      put_block(dev, bmap, buf);
      printf("allocated bno = %d\n", i);

      //get newly allocated block
      get_block(dev, i, buf);
      // zero out the block on disk
      for(int j = 0; j < BLKSIZE; j++)
      {
          buf[j] = 0;
      }
      // put zero'd out block back
      put_block(dev, i, buf);
      return i;
    }
  }
  printf(" -> Block was not allocated because no free blocks remain\n");
  return -1;
}
