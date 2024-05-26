#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

int start_insertion(int fd, char *rd_str, size_t str_sz)
{
	char *res_str = malloc(str_sz);
	memset(res_str, 0, str_sz);

	// Positionnement debut de fichier premiere ecriture
	lseek(fd, 0, SEEK_SET);
	if (write(fd, rd_str, str_sz) != str_sz) {
		dprintf(STDOUT_FILENO, "START_INSERTION : write error \n");
		exit(EXIT_FAILURE);
	}
	printf("rd_str : %s \n", rd_str);
	lseek(fd, 0, SEEK_SET);
	if ((read(fd, res_str, str_sz) != str_sz) &&
	    strncmp(rd_str, res_str, str_sz) == 0) {
		printf("res_str : %s \n", res_str);
		dprintf(STDOUT_FILENO,
			"First Write Successful in start_insertion \n");
	} else {
		printf("res_str : %s \n", res_str);
		dprintf(STDOUT_FILENO,
			"First Write Failed in start_insertion \n");
		return EXIT_FAILURE;
	}

	// Positionnement debut de fichier pour insertion
	lseek(fd, 0, SEEK_SET);
	char *nw_str = gen_string(str_sz);

	if (write(fd, nw_str, str_sz) != str_sz) {
		dprintf(STDOUT_FILENO, "START_INSERTION : write error \n");
		exit(EXIT_FAILURE);
	}

	// Positionnement nouveau bloc alloue verif ecriture
	lseek(fd, 4097, SEEK_SET);

	if ((read(fd, res_str, str_sz) != str_sz) &&
	    (strncmp(nw_str, res_str, str_sz) == 0)) {
		goto TEST_BLK_SPLIT;
	} else {
		dprintf(STDOUT_FILENO,
			"Second Write Failed in start_insertion \n");
		return EXIT_FAILURE;
	}

	// Positionnement bloc de SPLIT
	lseek(fd, 8193, SEEK_SET);

TEST_BLK_SPLIT:

	if ((read(fd, res_str, str_sz) != 0) &&
	    (strncmp(res_str, rd_str, str_sz) == 0)) {
		printf("START INSERTION : ");
		printf("\033[0;32m");
		printf("SUCCESS \n");
		printf("\033[0m");
		return EXIT_SUCCESS;
	} else {
		printf("START INSERTION : ");
		printf("\033[0;31m");
		printf("SUCCESS \n");
		printf("\033[0m");
		return EXIT_FAILURE;
	}
}

int rdwr_check(int fd, off_t offset, char *rd_str)
{
	return 0;
}

int main(int argc, char *argv[])
{
	int fd = open("test1", O_CREAT | O_RDWR | O_TRUNC, 0664);
	char *rd_str = gen_string(10);

	srand(time(NULL));
	off_t rd_off = rand() % 500;
	lseek(fd, 0, SEEK_SET);
	start_insertion(fd, rd_str, 10);

	return 0;
}
