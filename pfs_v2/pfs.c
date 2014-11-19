#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
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
	else						// process 파일들 처리 
	{
		struct stat stbuf2;
		char pid_str[100] = { 0, };
		char temp[1024] = { 0, };
		char *ptr = NULL;
		FILE *fp = NULL;

		stbuf->st_nlink = 1;
		stbuf->st_mode = S_IFREG | 0644;	// st_mode 0644 로 설정
		stbuf->st_atim = stbuf2.st_atim;	// 시간 속성 설정
		stbuf->st_mtim = stbuf2.st_mtim;
		stbuf->st_ctim = stbuf2.st_ctim;
		stbuf->st_uid = getuid();	// uid, gid 설정
		stbuf->st_gid = getgid();

		// pid 값만 뽑아낸다
		strcpy(pid_str, path + 1);
		ptr = strchr(pid_str, '-');
		*ptr = 0;
		sprintf(temp, "/proc/%s", pid_str);

		memset(&stbuf2, 0, sizeof(stbuf2));
		if(stat(temp, &stbuf2) == -1)
			return errno;

		// VmSize 읽기
		sprintf(temp, "/proc/%s/status", pid_str);
		fp = fopen(temp, "r");
		if(fp == NULL)
			return errno;

		while (fgets(temp, sizeof(temp), fp) != 0)
		{
			if(strncpy(temp, "VmSize:", 7) == 0)
			{
				int vmsize = 0;
				sscanf(temp + 7, "%d", &vmsize);

				// file size 설정 
				stbuf->st_size = vmsize;
				fclose(fp);
				return 0;
			}
		}

		fclose(fp);
		stbuf->st_size = 0;
	}

	return 0;
}

// 모든 c1 문자 c2로 치환
static void change_character(char *str, char c1, char c2)
{
	int len = strlen(str);
	int i;

	for(i = 0; i < len; i++)
	{
		if(str[i] == c1)
			str[i] = c2;
	}
}

static int is_digit_str(const char *str)
{
	int len = strlen(str);
	int i;

	for(i = 0; i < len; i++)
	{
		if(!isdigit(str[i]))
			return 0;			// false
	}

	return 1;					// true
}

static int pfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
	off_t offset, struct fuse_file_info *fi)
{
	struct dirent *dir_ent = NULL;
	DIR *dir = NULL;
	if(strcmp(path, "/") != 0)
		return -ENOENT;

	// proc 폴더의 모든 pid 처리
	dir = opendir("/proc");
	if(dir == NULL)
		return errno;

	while ((dir_ent = readdir(dir)) != NULL)
	{
		char temp[1024] = { 0, };
		char name[1024] = { 0, };
		FILE *fp = NULL;

		// 파일 이름이 숫자로 구성된 경우만 처리한다. 
		if(!is_digit_str(dir_ent->d_name))
			continue;

		// cmdline 파일을 읽어 파일명을 만든다.
		sprintf(temp, "/proc/%s/cmdline", dir_ent->d_name);
		fp = fopen(temp, "r");
		if(fp == NULL)
		{
			closedir(dir);
			return errno;
		}

		// cmdline 파일내용 읽기.
		if(fgets(temp, sizeof(temp), fp) == NULL)
		{
			fclose(fp);
			continue;
		}
		fclose(fp);

		// 파일이름 설정 --> pid-cmdline
		change_character(temp, '/', '-');
		sprintf(name, "%s-%s", dir_ent->d_name, temp[0] == '-' ? temp + 1 : temp);
		filler(buf, name, NULL, 0);
	}
	closedir(dir);

	return 0;
}

static int pfs_unlink(const char *path)
{
	char fname[1024] = { 0, };
	char *ptr = NULL;
	int pid = 0;

	if(strcmp(path, "/") == 0)
		return -ENOENT;

	strcpy(fname, path + 1);
	ptr = strchr(fname, '-');
	*ptr = 0;
	pid = atoi(fname);

	// SIGKILL 시그널을 보낸다
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
