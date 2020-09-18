#include <string.h>
#include <errno.h>
#include "common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <dirent.h>

/* make sure to use syserror() when a system call fails. see common.h */

void
usage()
{
	fprintf(stderr, "Usage: cpr srcdir dstdir\n");
	exit(1);
}

void fileCpy(char* SRC, char* DST, struct stat* statbuf){

	//open the file
	int fd;
	fd = open(SRC, O_RDONLY);
	if (fd < 0) syserror(open, SRC);

	//create a new file
	int nfd;
	nfd = open(DST, O_CREAT | O_RDWR);
	if (nfd < 0) syserror(open, DST);
	

	//copy the file to new file
	char buf[4096];
	int ret;
	do {
		ret = read(fd, buf, 4096);
		if (ret < 0) syserror(read, SRC);
		//write the buffer to new file
		if (write(nfd, buf, ret) < 0) syserror(write, DST);	
	} while (ret != 0);
	
	if (chmod(DST, statbuf->st_mode) < 0) syserror(chmod, DST);

	//close source file and new file
	if (close(fd) < 0) syserror(close, SRC);
	if (close(nfd) < 0) syserror(close, DST);
}

void dirCpy(char* SRC, char* DST){

	// //check whether it's just a file
	// struct stat* statbuf;
	// if (stat(SRC, statbuf) < 0) syserror(stat, SRC);
	// if (S_ISREG(statbuf->st_mode)) {
	// 	//if just a regular file
	// 	fileCpy(SRC, DST);
	// 	return;
	// }

	//open the directory
	DIR* dirp = opendir(SRC);
	if (dirp == NULL) syserror(opendir, SRC);

	//iterate directory entries
	struct dirent* entry;
	while ((entry = readdir(dirp)) != NULL){

		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;

		char* newDST = (char*)malloc(sizeof(char) * (strlen(DST) + 300));
		char* newSRC = (char*)malloc(sizeof(char) * (strlen(SRC) + 300));

		memset(newDST, 0, strlen(newDST));
		strcat(newDST, DST);
		strcat(newDST, "/");
		strcat(newDST, entry->d_name);

		memset(newSRC, 0, strlen(newSRC));
		strcat(newSRC, SRC);
		strcat(newSRC, "/");
		strcat(newSRC, entry->d_name);

		printf("%s\n%s\n\n", newSRC, newDST);
		//if the entry is a file
		struct stat* statbuf = (struct stat*)malloc(sizeof(struct stat));;
		if (stat(newSRC, statbuf) < 0) syserror(stat, newSRC);

		if (S_ISREG(statbuf->st_mode)) {
			//if just a regular file
			fileCpy(newSRC, newDST, statbuf);
		} else{
			//if the entry is a directory
			if (mkdir(newDST, S_IRWXU) < 0) syserror(mkdir, newDST);
			dirCpy(newSRC, newDST);
			if (chmod(newDST, statbuf->st_mode) < 0) syserror(chmod, newDST);
		}

		free(statbuf);
		free(newDST);
		free(newSRC);
	}

	if (closedir(dirp) < 0) syserror(closedir, SRC);

}

int
main(int argc, char *argv[])
{
	if (argc != 3) {
		usage();
	}
	char* SRC = argv[1];
	char* DST = argv[2];
	
	if (mkdir(DST, S_IRWXU) < 0) syserror(mkdir, DST);
		
	struct stat* statbuf = (struct stat*)malloc(sizeof(struct stat));
	if (stat(SRC, statbuf) < 0) syserror(stat, SRC);
	if (chmod(DST, statbuf->st_mode) < 0) syserror(chmod, DST);
	
	free(statbuf);

	dirCpy(SRC, DST);
	return 0;
}
