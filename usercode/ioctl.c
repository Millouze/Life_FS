#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define OUICHEFS_MAGIC 0x48434957

#define OUICH_FILE_INFO _IOR('N', 1, char *)

int main(int argc, char *argv[])
{
	int fd = open(argv[1], O_RDWR | O_CREAT, 0777);
	if (fd < 0) {
		printf("Erreur open\n");
		return EXIT_FAILURE;
	}
	char buf[100] = { 0 };
	if (ioctl(fd, OUICH_FILE_INFO, buf) < 0) {
		printf("Erreur IOCTL\n");
	}
}