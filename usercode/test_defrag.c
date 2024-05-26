#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	int fd = open("test_defarg", O_CREAT | O_RDWR | O_TRUNC, 0664);
	srand(time(NULL));
	char *rd_str = gen_string(16384); //4 blocks long string
	write(fd, rd_str, 16384);
	free(rd_str);

	printf("Original file info\n");
	ioctl(fd, OUICH_FILE_INFO);
	//Créer de la fragmentation. on se leseek dans le milieu du fichier
	lseek(fd, 5623, SEEK_SET);
	write(fd, "aa", strlen("aa"));

	printf("\n\nFile info after first insertion in the middle\n");

	ioctl(fd, OUICH_FILE_INFO);

	printf("\n\nFirst defrag\n");

	ioctl(fd, OUICH_FILE_DEFRAG);

	printf("\n\nFile info after first defrag\n");
	ioctl(fd, OUICH_FILE_INFO);

	lseek(fd, 0, SEEK_SET);

	write(fd, "aa", strlen("aa"));

	lseek(fd, 0, SEEK_SET);

	write(fd, "aa", strlen("aa"));

	printf("\n\nFile info after 2 insertions at 0\n");
	ioctl(fd, OUICH_FILE_INFO);

	//appel de la défragmentation

	ioctl(fd, OUICH_FILE_DEFRAG);

	printf("\n\nFile info after defrag");
	ioctl(fd, OUICH_FILE_INFO);
}
