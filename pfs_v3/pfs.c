#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <fuse.h>

// 파일 속성을 설정한다. 
static int pfs_getattr(const char *path, struct stat *stbuf)
{
	memset(stbuf, 0, sizeof(struct stat));
	if(strcmp(path, "/") == 0)
	{
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}

	/*
	   이하 pid - cmdline 파일 처리
	 */
	stbuf->st_nlink = 1;

	// st_mode 0644 로 설정
	stbuf->st_mode = S_IFREG | 0644;

	char pid[256];
	strcpy(pid, path + 1);
	char *p = strchr(pid, '-');
	if(p == NULL)
		return 0;
	*p = '\0';

	char temp[256];
	sprintf(temp, "/proc/%s", pid);

	struct stat st;
	memset(&st, 0, sizeof(st));
	if(stat(temp, &st) == -1)
		return errno;

	// set uid, gid
	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid();

	// set time attr
	stbuf->st_atim = st.st_atim;
	stbuf->st_mtim = st.st_mtim;
	stbuf->st_ctim = st.st_ctim;

	// read status file
	sprintf(temp, "/proc/%s/status", pid);
	FILE *fp = fopen(temp, "r");
	if(fp == NULL)
		return errno;

	while (fgets(temp, sizeof(temp), fp) != 0)
	{
		if(strncpy(temp, "VmSize:", 7) == 0)
		{
			int vmsize = 0;
			sscanf(temp + 7, "%d", &vmsize);

			// set file size to vmsize
			stbuf->st_size = vmsize;
			fclose(fp);
			return 0;
		}
	}

	// clean up
	fclose(fp);
	stbuf->st_size = 0;
	return 0;
}

// 모든 c1 문자 c2로 치환
static void replace(char *s, char a, char b)
{
	int l = strlen(s);
	for(int i = 0; i < l; i++)
		if(s[i] == a)
			s[i] = b;
}

// 모든 문자가 숫자 문자인지 여부 리턴 
static int is_digit_str(const char *s)
{
	while (*s)
		if(!isdigit(*s++))
			return 0;
	return 1;
}

static int pfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	if(strcmp(path, "/") != 0)
		return -ENOENT;

	// open /proc dir
	DIR *dir = opendir("/proc");
	if(dir == NULL)
		return errno;

	struct dirent *dent = NULL;
	while ((dent = readdir(dir)) != NULL)
	{
		// only pid file
		if(!is_digit_str(dent->d_name))
			continue;

		// read cmdline
		char temp[256];
		sprintf(temp, "/proc/%s/cmdline", dent->d_name);
		FILE *fp = fopen(temp, "r");
		if(fp == NULL)
		{
			closedir(dir);
			return errno;
		}

		if(fgets(temp, sizeof(temp), fp) == NULL)
		{
			fclose(fp);
			continue;
		}
		fclose(fp);

		// set file name to  pid-cmdline
		replace(temp, '/', '-');
		char name[256];
		sprintf(name, "%s-%s", dent->d_name, *temp == '-' ? temp + 1 : temp);
		filler(buf, name, NULL, 0);
	}
	closedir(dir);

	return 0;
}

static int pfs_unlink(const char *path)
{
	if(strcmp(path, "/") == 0)
		return -ENOENT;

	char name[256];
	strcpy(name, path + 1);
	char *p = strchr(name, '-');
	if(p == NULL)
		return 0;

	*p = '\0';
	int pid = atoi(name);

	// send SIGKILL 
	kill(pid, SIGKILL);
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
