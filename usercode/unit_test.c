#include "utils.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

int start_insertion(int fd, char *rd_str, size_t str_sz)
{
	int a = 0;
	int b = 0;
	char *res_str = malloc(str_sz);
	memset(res_str, 0, str_sz);

	// Positionnement debut de fichier premiere ecriture
	lseek(fd, 0, SEEK_SET);
	if (write(fd, rd_str, str_sz) != str_sz) {
		dprintf(STDOUT_FILENO, "START_INSERTION : write error \n");
		exit(EXIT_FAILURE);
	}
	lseek(fd, 0, SEEK_SET);
	a = read(fd, res_str, str_sz);
	b = strncmp(rd_str, res_str, str_sz);
	if ((a == str_sz) && (b == 0)) {
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
	lseek(fd, 0, SEEK_SET);

	a = read(fd, res_str, str_sz);
	b = strncmp(nw_str, res_str, str_sz);
	if ((a == str_sz) && (b == 0)) {
		printf("START INSERTION : ");
		printf(GREEN);
		printf("SUCCESS \n");
		printf(COLOR_DEFAULT);
		return EXIT_SUCCESS;
	} else {
		printf("START INSERTION : ");
		printf(RED);
		printf("FAILED \n");
		printf(COLOR_DEFAULT);
		return EXIT_FAILURE;
	}
}

// Test pour confirmer que l'ajout en fin de fichier avec espace insuffisant echoue
int end_insertion(int fd, char *rd_str, size_t str_sz)
{
	lseek(fd, FILE_MAXOFFSET - (BLK_SIZE * 2) - str_sz, SEEK_SET);
	if (write(fd, rd_str, str_sz) <= 0) {
		dprintf(STDOUT_FILENO, "Write Error : end_insertion \n");
		return EXIT_FAILURE;
	}

	lseek(fd, FILE_MAXOFFSET - (BLK_SIZE + str_sz), SEEK_SET);
	if (write(fd, rd_str, str_sz) <= 0) {
		if (errno == ENOSPC) {
			printf("END INSERTION : ");
			printf(GREEN);
			printf("FAILED SUCCESSFULLY\n");
			printf(COLOR_DEFAULT);
			return EXIT_SUCCESS;
		}
	} else {
		printf("END INSERTION : ");
		printf(RED);
		printf("INSERTED AT END OF FILE SOMEHOW\n");
		printf(COLOR_DEFAULT);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

// Ecrit et verifie l'ecriture
int write_at(int fd, off_t off, char *rd_str, size_t str_sz)
{
	int a = 0;
	int b = 0;
	char *res = malloc(str_sz);
	memset(res, 0, str_sz);
	lseek(fd, off, SEEK_SET);
	if (write(fd, rd_str, str_sz) <= 0) {
		dprintf(STDOUT_FILENO, "Write Error : end_insertion \n");
		return EXIT_FAILURE;
	}

	lseek(fd, off, SEEK_SET);
	a = read(fd, res, str_sz);
	b = strncmp(rd_str, res, str_sz);
	if (a != str_sz || b != 0) {
		dprintf(STDOUT_FILENO, "Read Wrong Value in spaced_wr \n");
		printf("WRITE AT : ");
		printf(RED);
		printf("FAILED\n");
		printf(COLOR_DEFAULT);
		return EXIT_FAILURE;
	}

	printf("WRITE AT : ");
	printf(GREEN);
	printf("SUCCESS\n");
	printf(COLOR_DEFAULT);
	return EXIT_SUCCESS;
}

// Test des ecritures espacees dans un fichier
int spaced_wr(int fd, off_t off_st, off_t off_end, char *rd_str, size_t str_sz)
{
	int a = 0;
	int b = 0;
	char *res = malloc(str_sz);
	memset(res, 0, str_sz);
	lseek(fd, off_st, SEEK_SET);
	if (write(fd, rd_str, str_sz) <= 0) {
		dprintf(STDOUT_FILENO, "Write Error : end_insertion \n");
		return EXIT_FAILURE;
	}

	lseek(fd, off_st, SEEK_SET);
	a = read(fd, res, str_sz);
	b = strncmp(rd_str, res, str_sz);
	if (a != str_sz || b != 0) {
		printf("res : %s %d %d\n", res, b, a);
		dprintf(STDOUT_FILENO, "Read Wrong Value in spaced_wr \n");
		return EXIT_FAILURE;
	}

	char *nw_str = gen_string(str_sz);
	lseek(fd, off_end, SEEK_SET);
	if (write(fd, nw_str, str_sz) <= 0) {
		dprintf(STDOUT_FILENO, "Write Error : end_insertion \n");
		return EXIT_FAILURE;
	}

	lseek(fd, off_end, SEEK_SET);
	a = read(fd, res, str_sz);
	b = strncmp(nw_str, res, str_sz);
	if (a != str_sz || b != 0) {
		printf("SPACED WRITE : ");
		printf(RED);
		printf("FAILED \n");
		printf(COLOR_DEFAULT);
		return EXIT_FAILURE;
	}

	printf("SPACED WRITE : ");
	printf(GREEN);
	printf("SUCCESS \n");
	printf(COLOR_DEFAULT);
	free(nw_str);

	return EXIT_SUCCESS;
}

//Test des écritures au milieu d'un fichier
int middle_insertion(int fd, char *str, size_t sz, off_t off)
{
	char *buf = (char *)calloc(sz, sizeof(char));
	lseek(fd, off, SEEK_SET);

	if (write(fd, str, sz) <= 0) {
		dprintf(STDOUT_FILENO, "Write Error : end_insertion \n");
		return EXIT_FAILURE;
	}
	lseek(fd, off, SEEK_SET);
	int a = read(fd, buf, sz);
	int b = strncmp(str, buf, sz);
	printf("a = %d \n%s, %s\n", a, str, buf);

	if (a != sz || b != 0) {
		printf("MIDDLE INSERTION : ");
		printf(RED);
		printf("FAILED \n");
		printf(COLOR_DEFAULT);
		free(buf);
		return EXIT_FAILURE;
	}

	printf("MIDDLE INSERTION : ");
	printf(GREEN);
	printf("SUCCESS \n");
	printf(COLOR_DEFAULT);
	free(buf);

	return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
	int fd1 = open("test1", O_CREAT | O_RDWR | O_TRUNC, 0664);
	int fd2 = open("test2", O_CREAT | O_RDWR | O_TRUNC, 0664);
	int fd3 = open("test3", O_CREAT | O_RDWR | O_TRUNC, 0664);
	int fd4 = open("test4", O_CREAT | O_RDWR | O_TRUNC, 0664);
	int fd5 = open("test5", O_CREAT | O_RDWR | O_TRUNC, 0664);
	srand(time(NULL));
	char *rd_str = gen_string(10);
	start_insertion(fd1, rd_str, 10);
	// end_insertion(fd2, rd_str, 10);
	spaced_wr(fd3, BLK_SIZE * 2, BLK_SIZE * 1000, rd_str, 10);
	// write_at(fd4, FILE_MAXOFFSET - (BLK_SIZE * 3) - 10, rd_str, 10);
	free(rd_str);
	close(fd1);
	close(fd2);
	close(fd3);
	close(fd4);
	return 0;
}
