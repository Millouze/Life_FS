#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int main()
{
	char pathname[10] = {0};
	for (int i = 0; i < 100; i++) {
		sprintf(pathname, "fichier%d", i);
		int fd = open(pathname, O_CREAT | O_RDWR, 0666);
		if (fd < 0) {
			printf("Erreur open %d\n", i);
			exit(EXIT_FAILURE);
		}

		for (int j = 0; j < 80; j++) {
			if (write(fd, "a", sizeof(char)) < 0) {
				printf("Erreur write\n");
				exit(EXIT_FAILURE);
			}
		}
		off_t off = lseek(fd, -20, SEEK_CUR);

		clock_t begin_time = clock();
		for (int j = 0; j < 10; j++) {
			if (write(fd, "b", sizeof(char)) < 0) {
				printf("Erreur write\n");
				exit(EXIT_FAILURE);
			}
		}
		time_t end_time = clock();
		printf("Temps d'écriture au milieu %ld\n", end_time - begin_time);
		close(fd);
	}
	return 0;
}