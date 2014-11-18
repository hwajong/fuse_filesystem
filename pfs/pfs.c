#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

static int pfs_getattr(const char *path, struct stat *stbuf)
{
	char pid[512] = {0,};
	char fname[512] = {0,};
	char vmsize[1024] = {0,};
	char *p = NULL;
	struct stat statbuf;
	FILE *fp = NULL;

	memset(stbuf, 0, sizeof(struct stat));
	if(strcmp(path, "/") == 0)
	{
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	}
	else
	{
		stbuf->st_mode = S_IFREG | 0644;
		stbuf->st_nlink = 1;
		stbuf->st_uid = getuid();
		stbuf->st_gid = getgid();

		strcpy(pid, path+1);
		p = strchr(pid, '-');
		*p = 0;

		memset(&statbuf, 0, sizeof(statbuf));

		sprintf(fname, "/proc/%s", pid);
		stat(fname, &statbuf);
		stbuf->st_atim = statbuf.st_atim;
		stbuf->st_mtim = statbuf.st_mtim;
		stbuf->st_ctim = statbuf.st_ctim;

		sprintf(fname, "/proc/%s/status", pid);
		fp = fopen(fname, "r");
		if(fp == NULL) 
		{   
			return -errno;
		}   

		while(fgets(vmsize, sizeof(vmsize), fp) != 0)
		{   
			if(memcmp(vmsize, "VmSize:", 7) == 0)
			{   
				int size = 0;
				sscanf(vmsize+7, "%d", &size);
				stbuf->st_size = size;
				return 0;
			}
		}

		stbuf->st_size = 0;
	}

	return 0;
}

static void replace_char(char* s, char t, char c)
{
	while(*s)
	{   
		if(*s == t) *s = c;
		s++;
	}   
}


static int pfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	DIR *dir = NULL;
	struct dirent *ent = NULL;
	char fname[512] = {0,};
	char cmdline[512] = {0,};
	char pid_name[512] = {0,};
	FILE *fp = NULL;

	(void)offset;
	(void)fi;

	if(strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	
	dir = opendir("/proc");
	if(dir == NULL)
	{
		perror("opendir(/proc)");
		return -errno;
	}


	while((ent = readdir(dir)) != NULL)
	{
		if(!isdigit(ent->d_name[0])) continue;

		sprintf(fname, "/proc/%s/cmdline", ent->d_name);
		fp = fopen(fname, "r");
		if(fp == NULL) return -errno;

		if(fgets(cmdline, sizeof(cmdline), fp) == NULL)
		{
			fclose(fp);
			continue;
		}
		fclose(fp);

		printf("%s-%s\n", ent->d_name, cmdline[0] == '-' ? cmdline+1 : cmdline);

		replace_char(cmdline, '/', '-');
		sprintf(pid_name, "%s-%s", ent->d_name, cmdline[0] == '-' ? cmdline+1 : cmdline);
		filler(buf, pid_name, NULL, 0);
	}
	closedir(dir);

	return 0;
}

static int pfs_unlink(const char *path)
{
	char pid[512] = {0,};
	char *p = NULL;
	int pid_int = 0;
	
	if(strcmp(path, "/") == 0)
	{
		return -ENOENT;
	}

	strcpy(pid, path+1);
	p = strchr(pid, '-');
	*p = 0;

	pid_int = atoi(pid);
	kill(pid_int, SIGKILL);

	return 0;
}

static struct fuse_operations pfs_oper = {
	.getattr = pfs_getattr,
	.readdir = pfs_readdir,
	.unlink = pfs_unlink
};

int main(int argc, char **argv)
{
	return fuse_main(argc, argv, &pfs_oper);
}
