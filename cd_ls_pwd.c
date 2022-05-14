/************* cd_ls_pwd.c file **************/

extern int dev;
extern MINODE *root;
extern PROC *running;

int ch_dir(char *pathname)   
{
  int ino = getino(pathname);
  if(ino == 0){
	printf(" -> Directory does not exist\n");
  	return -1;
  }
  
  MINODE *mip = iget(dev, ino);
  if(!S_ISDIR(mip->INODE.i_mode)){
	printf(" -> not a directory!\n");
	iput(mip);
  	return -1;
  }
  iput(running->cwd);
  running->cwd = mip;
}

void print_info(MINODE *mip, char *name)
{
	int i;
	INODE *ip = &mip->INODE;

	char *permission = "rwxrwxrwx";

  u16 uid    = ip->i_uid;           // owner uid
  u16 gid    = ip->i_gid;           // group id
  u32 dev    = mip->dev;            // device number
  u32 size   = ip->i_size;          // size in bytes
  u16 mode   = ip->i_mode;          // DIR type, permissions
  u16 links  = ip->i_links_count;   // links count
	int ino = mip->ino;

  switch(mode&0xF000)
	{
    case 0x4000:  putchar('d');     break;
    case 0xA000:  putchar('l');     break; 
		case 0x8000:  putchar('-');     break;
    default:      putchar('?');     break;
	}
	const time_t timeptr= mip->INODE.i_mtime;
    char *time_str = ctime(&timeptr);

	time_str[strlen(time_str) - 1] = 0; // remove carriage return

	for(i = 0; i < strlen(permission); i++)
  {
    putchar(mode & (1 << (strlen(permission) - 1 - i)) ? permission[i] : '-');
  }

	printf("%7hu %4hu %4d %4d %4hu %8u %26s  %s", links, gid, uid, dev, ino, size, time_str, name);

	S_ISLNK(mode)?printf(" -> %s\n", (char *)ip->i_block):putchar('\n');



}

void print_directory(MINODE *mip)
{
	int i;
	char *cp;
	char buf[1024], temp_str[1024];

  DIR *dp;
	INODE *ip = &mip->INODE;
	MINODE *temp_mip;

	printf("\n  MODE      LINKS  GID  UID  DEV  INO     SIZE          MODIFIED           NAME\n");

	for(i = 0; i < ip->i_size/1024; i++)
	{
		if(ip->i_block[i] == 0)
			break;

		get_block(mip->dev, ip->i_block[i], buf);
		dp = (DIR*)buf;
		cp = buf;

		while(cp < buf + BLKSIZE)
		{
			strncpy(temp_str, dp->name, dp->name_len);
			temp_str[dp->name_len] = 0;

			temp_mip = iget(mip->dev, dp->inode);
			if(temp_mip)
			{
				print_info(temp_mip, temp_str);
				iput(temp_mip);
			}
			else
				printf("MINODE : cannot print info\n");

			memset(temp_str, 0, 1024);
			cp += dp->rec_len;
			dp = (DIR*)cp;
		}
	}

	printf("\n");
}

void ls(char *pathname)
{
	int ino, offset;
	MINODE *mip = running->cwd;
	char name[64][64], temp[64];
	char buf[1024];


	if(!strcmp(pathname, "/")) // root
	{
		print_directory(root);
		return;
	}
	else if(!strcmp(pathname, "")) // cwd
	{
		print_directory(mip);
		return;
	}
	else if(pathname)
	{
		if(pathname[0] == '/')
		{
			mip = root;
		}

		ino = getino(pathname);
		if(ino == 0)
		{
			return;
		}

		mip = iget(dev, ino);
		if(!S_ISDIR(mip->INODE.i_mode))
		{
			printf(" -> %s not a directory\n", pathname);
			iput(mip);
			return;
		}

		print_directory(mip);
		iput(mip);
	}
	else // is a directory
	{
		print_directory(root);
	}
}

int rpwd(MINODE *wd)
{
	char dirname[256];
	int ino, pino, i;
	MINODE *pip;

	if ((wd->dev != root->dev) && (wd->ino == 2))
	{
        // Find entry in mount table
        for(i = 0; i < MT_SIZE; i++)
        {
           if(mtable[i].dev  == wd->dev)
           {
              break;
           }
        }
        wd = mtable[i].mntDirPtr;
	}
	else if (wd == root){
		return 0;
	}

	pino = findino(wd, &ino);
	pip = iget(wd->dev, pino);

	findmyname(pip, ino, dirname);
	rpwd(pip);
	iput(pip);
	printf("/%s", dirname);
}


int pwd(MINODE *wd)
{
	if (wd == root){
		printf("/\n");
	}
	else
	{
		rpwd(wd);
	}
}
