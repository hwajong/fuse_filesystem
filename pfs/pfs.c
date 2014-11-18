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

// 파일 속성 셋팅.
static int pfs_getattr(const char *path, struct stat *stbuf) {
	char pid[512] = { 0, };
	char fname[512] = { 0, };
	char vmsize[1024] = { 0, };
	char *p = NULL;
	FILE *fp = NULL;
	struct stat statbuf;

	memset(stbuf, 0, sizeof(struct stat));
	if(strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}

	// 파일모드를 0644 로 셋팅.
	stbuf->st_mode = S_IFREG | 0644;
	stbuf->st_nlink = 1;

	// 사용자, 그룹을 현제 사용자, 그룹으로 셋팅.
	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid();

	// path 에서 pid 문자열만 추출.
	strcpy(pid, path + 1);
	p = strchr(pid, '-');
	*p = 0;
	sprintf(fname, "/proc/%s", pid);

	memset(&statbuf, 0, sizeof(statbuf));
	if(stat(fname, &statbuf) == -1)
		return -errno;

	// 시간 속성을 프로세스의 것으로 셋팅.
	stbuf->st_atim = statbuf.st_atim;
	stbuf->st_mtim = statbuf.st_mtim;
	stbuf->st_ctim = statbuf.st_ctim;

	// VmSize 를 읽기 위해 status 파일 오픈.
	sprintf(fname, "/proc/%s/status", pid);
	fp = fopen(fname, "r");
	if(fp == NULL)
		return -errno;

	while (fgets(vmsize, sizeof(vmsize), fp) != 0) {
		if(memcmp(vmsize, "VmSize:", 7) == 0) {
			int size = 0;
			sscanf(vmsize + 7, "%d", &size);

			// 파일 사이즈를 vmsize 로 셋팅.
			stbuf->st_size = size;
			fclose(fp);
			return 0;
		}
	}

	fclose(fp);
	stbuf->st_size = 0;
	return 0;
}

// 문자열에서 t 문자를 모두 c 문자로 변경한다. 
static void replace_char(char *s, char t, char c) {
	while (*s) {
		if(*s == t)
			*s = c;
		s++;
	}
}

// 파일 목록 셋팅.
static int pfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
	off_t offset, struct fuse_file_info *fi) {
	DIR *dir = NULL;
	struct dirent *ent = NULL;
	char fname[512] = { 0, };
	char cmdline[512] = { 0, };
	char pid_name[512] = { 0, };
	FILE *fp = NULL;

	(void)offset;
	(void)fi;

	if(strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	// proc 디렉토리 오픈
	dir = opendir("/proc");
	if(dir == NULL)
		return -errno;

	// proc 디렉토리의 모든 파일을 돌면서 처리.
	while ((ent = readdir(dir)) != NULL) {
		// 첫글자가 숫자인 경우만 처리.
		if(!isdigit(ent->d_name[0]))
			continue;

		// cmdline 파일 오픈
		sprintf(fname, "/proc/%s/cmdline", ent->d_name);
		fp = fopen(fname, "r");
		if(fp == NULL) {
			closedir(dir);
			return -errno;
		}

		// cmdline 파일내용 읽기.
		if(fgets(cmdline, sizeof(cmdline), fp) == NULL) {
			fclose(fp);
			continue;
		}
		fclose(fp);

		// 파일이름 셋팅 : pid-cmdline
		printf("%s-%s\n", ent->d_name,
			cmdline[0] == '-' ? cmdline + 1 : cmdline);
		replace_char(cmdline, '/', '-');
		sprintf(pid_name, "%s-%s", ent->d_name,
			cmdline[0] == '-' ? cmdline + 1 : cmdline);
		filler(buf, pid_name, NULL, 0);
	}
	closedir(dir);

	return 0;
}

// rm 처리
static int pfs_unlink(const char *path) {
	char fname[512] = { 0, };
	char *p = NULL;
	int pid = 0;

	if(strcmp(path, "/") == 0)
		return -ENOENT;

	// path 에서 pid 문자열을 추출해 int 로 변환.
	strcpy(fname, path + 1);
	p = strchr(fname, '-');
	*p = 0;
	pid = atoi(fname);

	// kill 시그널 전송.
	kill(pid, SIGKILL);

	return 0;
}

static struct fuse_operations pfs_oper = {
	.getattr = pfs_getattr,
	.readdir = pfs_readdir,
	.unlink = pfs_unlink
};

int main(int argc, char **argv) {
	return fuse_main(argc, argv, &pfs_oper);
}
